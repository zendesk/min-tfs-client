/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/jit/kernels/xla_ops.h"

#include "absl/memory/memory.h"
#include "tensorflow/compiler/jit/defs.h"
#include "tensorflow/compiler/tf2xla/shape_util.h"
#include "tensorflow/compiler/tf2xla/tf2xla_util.h"
#include "tensorflow/compiler/tf2xla/xla_compiler.h"
#include "tensorflow/compiler/tf2xla/xla_op_registry.h"
#include "tensorflow/compiler/xla/client/client_library.h"
#include "tensorflow/compiler/xla/client/local_client.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/core/common_runtime/dma_helper.h"
#include "tensorflow/core/common_runtime/function.h"
#include "tensorflow/core/framework/allocator.h"
#include "tensorflow/core/framework/node_def_util.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/kernels/variable_ops.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/stream_executor_no_cuda.h"
#include "tensorflow/core/util/stream_executor_util.h"

namespace tensorflow {

namespace {

Status PlatformInfoFromContext(OpKernelConstruction* ctx,
                               XlaPlatformInfo* result) {
  DeviceType device_type = ctx->device_type();
  se::Platform::Id platform_id = nullptr;
  const XlaDevice::Metadata* xla_device_metadata = nullptr;
  std::unique_ptr<XlaAllocator> xla_allocator;
  xla::DeviceMemoryAllocator* device_allocator = nullptr;

  if (ctx->device_type() == DeviceType(DEVICE_CPU)) {
    platform_id = se::host::kHostPlatformId;
  } else if (ctx->device_type() == DeviceType(DEVICE_GPU)) {
    platform_id = ctx->device()
                      ->tensorflow_gpu_device_info()
                      ->stream->parent()
                      ->platform()
                      ->id();
  } else if (XlaDevice::GetMetadata(ctx, &xla_device_metadata).ok()) {
    // If we are on an XlaDevice, use the underlying XLA platform's allocator
    // directly. We could use the StreamExecutor's allocator which may
    // theoretically be more correct, but XLA returns a nice OOM message in a
    // Status and StreamExecutor does not.
    //
    // Importantly we can't use ctx->device()->GetAllocator() as the allocator
    // (which xla_allocator above uses) as on an XlaDevice, this is a dummy
    // allocator that returns XlaTensor objects. The XlaCompiler needs a real
    // allocator to allocate real buffers.

    platform_id = xla_device_metadata->platform()->id();
    device_allocator =
        xla_device_metadata->client()->backend().memory_allocator();
  }

  if (!device_allocator) {
    TF_ASSIGN_OR_RETURN(se::Platform* const platform,
                        se::MultiPlatformManager::PlatformWithId(platform_id));
    xla_allocator = absl::make_unique<XlaAllocator>(
        platform, ctx->device()->GetAllocator({}));
  }

  *result = XlaPlatformInfo(device_type, platform_id, xla_device_metadata,
                            std::move(xla_allocator), device_allocator);

  return Status::OK();
}

// A closure describing how to run a compiled version of a TensorFlow function.
//
// It may seem unusual to stick the resource variable snapshots in this class.
// This is necessary: we need to use the snapshots observed by the compiler as
// the initial values for the resource variables (and cannot snapshot them again
// during execution) because otherwise we risk observing a different snapshot
// with shapes different from what we compiled for.
class XlaExecutableClosure {
 public:
  explicit XlaExecutableClosure(
      xla::LocalClient* client, xla::LocalExecutable* executable,
      const XlaCompiler::CompilationResult* compilation_result,
      std::map<int, OptionalTensor> resource_var_snapshots)
      : client_(client),
        executable_(executable),
        compilation_result_(compilation_result),
        resource_var_snapshots_(std::move(resource_var_snapshots)) {}

  XlaExecutableClosure(XlaExecutableClosure&&) = default;
  XlaExecutableClosure& operator=(XlaExecutableClosure&&) = default;

  xla::LocalClient* client() const { return client_; }
  xla::LocalExecutable* executable() const { return executable_; }
  const XlaCompiler::CompilationResult* compilation_result() const {
    return compilation_result_;
  }
  const std::map<int, OptionalTensor>& resource_var_snapshots() const {
    return resource_var_snapshots_;
  }

 private:
  xla::LocalClient* client_;
  xla::LocalExecutable* executable_;
  const XlaCompiler::CompilationResult* compilation_result_;
  std::map<int, OptionalTensor> resource_var_snapshots_;

  TF_DISALLOW_COPY_AND_ASSIGN(XlaExecutableClosure);
};

// This maintains a mapping from a globally unique ID to XlaExecutableClosure
// instances.
class XlaExecutableClosureStore {
 public:
  XlaExecutableClosureStore() : key_counter_(0) {}

  using KeyT = string;

  KeyT Produce(XlaExecutableClosure result) {
    mutex_lock l(mutex_);
    KeyT key = absl::StrCat(key_counter_++);
    bool insert_successful = closures_.emplace(key, std::move(result)).second;
    DCHECK(insert_successful);
    (void)insert_successful;
    return key;
  }

  XlaExecutableClosure Consume(const KeyT& key) {
    mutex_lock l(mutex_);
    auto it = closures_.find(key);
    DCHECK(it != closures_.end());
    XlaExecutableClosure value = std::move(it->second);
    closures_.erase(it);
    return value;
  }

  static XlaExecutableClosureStore* Global() {
    static XlaExecutableClosureStore* instance = new XlaExecutableClosureStore;
    return instance;
  }

 private:
  mutex mutex_;
  int64 key_counter_ GUARDED_BY(mutex_);
  gtl::FlatMap<KeyT, XlaExecutableClosure> closures_ GUARDED_BY(mutex_);

  TF_DISALLOW_COPY_AND_ASSIGN(XlaExecutableClosureStore);
};

}  // namespace

XlaLocalLaunchBase::XlaLocalLaunchBase(OpKernelConstruction* ctx,
                                       const std::vector<int>& constants,
                                       const std::vector<int>& resources,
                                       const NameAttrList& function)
    : OpKernel(ctx),
      constants_(constants),
      resources_(resources),
      function_(function) {
  OP_REQUIRES_OK(ctx, PlatformInfoFromContext(ctx, &platform_info_));
}

static Status BuildCompilationCache(OpKernelContext* ctx,
                                    const XlaPlatformInfo& platform_info,
                                    XlaCompilationCache** cache) {
  if (platform_info.xla_device_metadata()) {
    *cache = new XlaCompilationCache(
        platform_info.xla_device_metadata()->client(),
        platform_info.xla_device_metadata()->jit_device_type());
    return Status::OK();
  }

  auto platform =
      se::MultiPlatformManager::PlatformWithId(platform_info.platform_id());
  if (!platform.ok()) {
    return platform.status();
  }
  xla::LocalClientOptions client_options;
  client_options.set_platform(platform.ValueOrDie());
  client_options.set_intra_op_parallelism_threads(
      ctx->device()->tensorflow_cpu_worker_threads()->num_threads);
  auto client = xla::ClientLibrary::GetOrCreateLocalClient(client_options);
  if (!client.ok()) {
    return client.status();
  }
  const XlaOpRegistry::DeviceRegistration* registration;
  if (!XlaOpRegistry::GetCompilationDevice(platform_info.device_type().type(),
                                           &registration)) {
    return errors::InvalidArgument("No JIT device registered for ",
                                   platform_info.device_type().type());
  }
  *cache = new XlaCompilationCache(
      client.ValueOrDie(), DeviceType(registration->compilation_device_name));
  return Status::OK();
}

static Status CompileToLocalExecutable(
    OpKernelContext* ctx, const NameAttrList& function,
    const XlaPlatformInfo& platform_info, absl::Span<const int> resources,
    absl::Span<const int> constants, xla::LocalClient** client,
    std::map<int, OptionalTensor>* variables,
    const XlaCompiler::CompilationResult** kernel,
    xla::LocalExecutable** executable) {
  // We store information about the JIT-compiled XLA computation
  // in the ResourceMgr.
  ResourceMgr* rm = ctx->resource_manager();
  if (!rm) {
    return errors::Internal("No resource manager.");
  }

  XlaCompilationCache* cache;
  TF_RETURN_IF_ERROR(rm->LookupOrCreate<XlaCompilationCache>(
      rm->default_container(), "xla_cache", &cache,
      [&](XlaCompilationCache** cache) {
        return BuildCompilationCache(ctx, platform_info, cache);
      }));
  // Hold the reference to the JIT during evaluation. (We could probably
  // free it sooner because the ResourceMgr will retain a reference, but
  // this is more obviously correct.)
  core::ScopedUnref cache_ref(cache);

  *variables = SnapshotResourceVariables(ctx, resources);
  *client = static_cast<xla::LocalClient*>(cache->client());

  XlaCompiler::Options options;
  options.client = *client;
  if (ctx->op_device_context() != nullptr) {
    options.device_ordinal =
        ctx->op_device_context()->stream()->parent()->device_ordinal();
  }
  options.device_type = cache->device_type();
  options.flib_def = ctx->function_library()->GetFunctionLibraryDefinition();
  options.graph_def_version = ctx->function_library()->graph_def_version();
  options.allow_cpu_custom_calls =
      (platform_info.platform_id() == se::host::kHostPlatformId);
  options.device_allocator = platform_info.allocator();
  if (platform_info.xla_device_metadata()) {
    options.shape_representation_fn =
        platform_info.xla_device_metadata()->shape_representation_fn();
  }

  std::map<int, Tensor> constant_args;
  for (int i : constants) {
    constant_args.insert({i, ctx->input(i)});
  }
  XlaCompiler::CompileOptions compile_options;
  compile_options.is_entry_computation = true;
  // If we resolve constants we never emit them on the device, meaning that if
  // they are needed by a following computation the host has to transfer
  // them. Not resolving constants is expected to be faster than resolving
  // constants.
  compile_options.resolve_compile_time_constants = true;
  // Optimization: where possible, have the computation return a naked array
  // rather than a one-element tuple.
  compile_options.always_return_tuple = false;

  return cache->Compile(options, function, constant_args, *variables, ctx,
                        kernel, executable, compile_options);
}

void XlaLocalLaunchBase::Compute(OpKernelContext* ctx) {
  VLOG(1) << "XlaLocalLaunchOpBase::Compute "
          << Canonicalize(function_.name(), AttrSlice(&function_.attr()));

  xla::LocalClient* client;
  const XlaCompiler::CompilationResult* kernel;
  xla::LocalExecutable* executable;
  std::map<int, OptionalTensor> variables;

  OP_REQUIRES_OK(
      ctx, CompileToLocalExecutable(ctx, function_, platform_info_, resources_,
                                    constants_, &client, &variables, &kernel,
                                    &executable));

  se::Stream* stream =
      ctx->op_device_context() ? ctx->op_device_context()->stream() : nullptr;

  VLOG(1) << "Executing XLA Computation...";

  XlaComputationLaunchContext launch_context(
      client, platform_info_.allocator(),
      /*allocate_xla_tensors=*/platform_info_.is_on_xla_device(),
      platform_info_.UseMultipleStreams());
  launch_context.PopulateInputs(ctx, kernel, variables);

  // Execute the computation.
  VLOG(2) << "Executing computation.";
  xla::ExecutableRunOptions run_options;
  run_options.set_stream(stream);
  run_options.set_allocator(platform_info_.allocator());
  run_options.set_intra_op_thread_pool(&ctx->eigen_cpu_device());
  run_options.set_rng_seed(GetXLARandomSeed());
  Env* env = Env::Default();
  auto start_time = env->NowMicros();

  auto run_result = executable->Run(launch_context.arguments(), run_options);
  OP_REQUIRES(ctx, run_result.ok(), run_result.status());

  auto elapsed = env->NowMicros() - start_time;
  VLOG(2) << "Elapsed time: " << elapsed << "us";

  OP_REQUIRES_OK(ctx, launch_context.PopulateOutputs(
                          ctx, kernel, run_result.ConsumeValueOrDie()));
  VLOG(1) << "Done";
}

namespace {

// OP_REQUIRES_OK_RETURN is the same as OP_REQUIRES_OK except that
// in error case, it returns RET instead of void.
#define OP_REQUIRES_OK_RETURN(CTX, RET, ...)                \
  do {                                                      \
    ::tensorflow::Status _s(__VA_ARGS__);                   \
    if (!TF_PREDICT_TRUE(_s.ok())) {                        \
      (CTX)->CtxFailureWithWarning(__FILE__, __LINE__, _s); \
      return RET;                                           \
    }                                                       \
  } while (0)

// Helper static functions to construct parameters for
// XlaLocalLaunchBase constructor from OpKernelConstruction.
std::vector<int> ConstantsVector(OpKernelConstruction* ctx) {
  DataTypeVector constant_types;
  OP_REQUIRES_OK_RETURN(ctx, std::vector<int>(),
                        ctx->GetAttr("Tconstants", &constant_types));
  std::vector<int> constants(constant_types.size());
  std::iota(constants.begin(), constants.end(), 0);
  return constants;
}

std::vector<int> ResourcesVector(OpKernelConstruction* ctx) {
  DataTypeVector constant_types;
  OP_REQUIRES_OK_RETURN(ctx, std::vector<int>(),
                        ctx->GetAttr("Tconstants", &constant_types));

  DataTypeVector arg_types;
  OP_REQUIRES_OK_RETURN(ctx, std::vector<int>(),
                        ctx->GetAttr("Targs", &arg_types));

  int num_resources;
  OP_REQUIRES_OK_RETURN(ctx, std::vector<int>(),
                        ctx->GetAttr("Nresources", &num_resources));

  std::vector<int> resources(num_resources);
  std::iota(resources.begin(), resources.end(),
            constant_types.size() + arg_types.size());
  return resources;
}

NameAttrList FunctionAttr(OpKernelConstruction* ctx) {
  const NameAttrList* func;
  OP_REQUIRES_OK_RETURN(ctx, NameAttrList(), ctx->GetAttr("function", &func));
  return *func;
}

#undef OP_REQUIRES_OK_RETURN
}  // namespace

XlaLocalLaunchOp::XlaLocalLaunchOp(OpKernelConstruction* ctx)
    : XlaLocalLaunchBase(ctx, ConstantsVector(ctx), ResourcesVector(ctx),
                         FunctionAttr(ctx)) {}

XlaLocalLaunchOp::~XlaLocalLaunchOp() {
  VLOG(1) << "XlaLocalLaunchOp destroyed";
}

XlaCompileOp::XlaCompileOp(OpKernelConstruction* ctx)
    : OpKernel(ctx),
      constants_(ConstantsVector(ctx)),
      resources_(ResourcesVector(ctx)),
      function_(FunctionAttr(ctx)) {
  OP_REQUIRES_OK(ctx, PlatformInfoFromContext(ctx, &platform_info_));
}

void XlaCompileOp::Compute(OpKernelContext* ctx) {
  xla::LocalClient* client;
  const XlaCompiler::CompilationResult* kernel;
  xla::LocalExecutable* executable;
  std::map<int, OptionalTensor> variables;

  OP_REQUIRES_OK(
      ctx, CompileToLocalExecutable(ctx, function_, platform_info_, resources_,
                                    constants_, &client, &variables, &kernel,
                                    &executable));

  // Each execution of an XlaCompile op creates a new XlaExecutableClosure, even
  // if it didn't have to compile the cluster because of a compilation-cache
  // hit.  This is because we at least need new snapshots of the resource
  // variables.
  XlaExecutableClosureStore::KeyT key =
      XlaExecutableClosureStore::Global()->Produce(XlaExecutableClosure(
          client, executable, kernel, std::move(variables)));

  Allocator* cpu_allocator = [&] {
    AllocatorAttributes host_alloc_attrs;
    host_alloc_attrs.set_gpu_compatible(true);
    host_alloc_attrs.set_on_host(true);
    return ctx->device()->GetAllocator(host_alloc_attrs);
  }();

  Tensor compilation_key(cpu_allocator, DT_STRING, TensorShape({}));
  compilation_key.flat<string>()(0) = key;

  Tensor compilation_successful(cpu_allocator, DT_BOOL, TensorShape({}));
  compilation_successful.flat<bool>()(0) = true;

  ctx->set_output(0, compilation_key);
  ctx->set_output(1, compilation_successful);
}

XlaRunOp::XlaRunOp(OpKernelConstruction* ctx) : OpKernel(ctx) {
  OP_REQUIRES_OK(ctx, PlatformInfoFromContext(ctx, &platform_info_));
}

void XlaRunOp::Compute(OpKernelContext* ctx) {
  Tensor key_tensor = ctx->input(ctx->num_inputs() - 1);
  const XlaExecutableClosureStore::KeyT& key = key_tensor.flat<string>()(0);

  XlaExecutableClosure closure =
      XlaExecutableClosureStore::Global()->Consume(key);

  XlaComputationLaunchContext launch_context(
      closure.client(), platform_info_.allocator(),
      /*allocate_xla_tensors=*/platform_info_.is_on_xla_device(),
      /*use_multiple_streams=*/platform_info_.UseMultipleStreams());
  launch_context.PopulateInputs(ctx, closure.compilation_result(),
                                closure.resource_var_snapshots());

  se::Stream* stream =
      ctx->op_device_context() ? ctx->op_device_context()->stream() : nullptr;
  xla::ExecutableRunOptions run_options;
  run_options.set_stream(stream);
  run_options.set_allocator(platform_info_.allocator());
  run_options.set_intra_op_thread_pool(&ctx->eigen_cpu_device());
  run_options.set_rng_seed(GetXLARandomSeed());
  Env* env = Env::Default();
  auto start_time = env->NowMicros();

  auto run_result =
      closure.executable()->Run(launch_context.arguments(), run_options);
  OP_REQUIRES(ctx, run_result.ok(), run_result.status());

  auto elapsed = env->NowMicros() - start_time;
  VLOG(2) << "Elapsed time in computation: " << elapsed << "us";

  OP_REQUIRES_OK(
      ctx, launch_context.PopulateOutputs(ctx, closure.compilation_result(),
                                          run_result.ConsumeValueOrDie()));
}

REGISTER_KERNEL_BUILDER(Name("XlaLaunch").Device(DEVICE_CPU), XlaLocalLaunchOp);

REGISTER_KERNEL_BUILDER(Name("XlaLaunch")
                            .Device(DEVICE_GPU)
                            .HostMemory("constants")
                            .HostMemory("resources"),
                        XlaLocalLaunchOp);

REGISTER_KERNEL_BUILDER(Name("_XlaCompile").Device(DEVICE_CPU), XlaCompileOp);
REGISTER_KERNEL_BUILDER(Name("_XlaCompile")
                            .Device(DEVICE_GPU)
                            .HostMemory("constants")
                            .HostMemory("resources"),
                        XlaCompileOp);

REGISTER_KERNEL_BUILDER(Name("_XlaRun").Device(DEVICE_CPU), XlaRunOp);

REGISTER_KERNEL_BUILDER(
    Name("_XlaRun").Device(DEVICE_GPU).HostMemory("constants"), XlaRunOp);

}  // namespace tensorflow
