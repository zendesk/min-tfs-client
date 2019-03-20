/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_CORE_COMMON_RUNTIME_FUNCTION_H_
#define TENSORFLOW_CORE_COMMON_RUNTIME_FUNCTION_H_

#include <functional>
#include <memory>

#include "tensorflow/core/common_runtime/device.h"
#include "tensorflow/core/common_runtime/device_mgr.h"
#include "tensorflow/core/common_runtime/graph_optimizer.h"
#include "tensorflow/core/common_runtime/process_function_library_runtime.h"
#include "tensorflow/core/framework/function.h"
#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/protobuf/config.pb.h"

namespace tensorflow {

static constexpr const char* const kNoInlineAttr = "_noinline";

// Registers a default customizable kernel creator for a function call.
//
// If 'cb()' returns a non-OK, we still fall back to an executor-based
// interpreter op kernel to execute a function. If 'cb()' returns OK,
// takes ownership of the returned OpKernel.
//
// TODO(zhifengc/phawkins): b/32379046
void RegisterDefaultCustomKernelCreator(CustomKernelCreator cb);

// Creates a FunctionLibraryRuntime, which instantiates functions
// defined in "lib_def" and executes functions on the "device".
// "device_mgr" must contain the "device". If not nullptr,
// "custom_kernel_creator" is consulted by the returned runtime to
// create kernels.
//
// The returned object does not take ownerships of "device" or
// "lib_def".  The caller must ensure "device" and "lib_def" outlives
// the returned object.
//
// The "parent" is a pointer to the ProcessFunctionLibraryRuntime object that
// typically owns the created FunctionLibraryRuntime object. The parent pointer
// is not owned by the FunctionLibraryRuntime object.
std::unique_ptr<FunctionLibraryRuntime> NewFunctionLibraryRuntime(
    const DeviceMgr* device_mgr, Env* env, Device* device,
    int graph_def_version, const FunctionLibraryDefinition* lib_def,
    thread::ThreadPool* thread_pool, const OptimizerOptions& optimizer_options,
    CustomKernelCreator custom_kernel_creator,
    ProcessFunctionLibraryRuntime* parent);

// Same as above except that the returned runtime consults with the
// global default custom kernel creator registered by
// RegisterDefaultCustomKernelCreator.
std::unique_ptr<FunctionLibraryRuntime> NewFunctionLibraryRuntime(
    const DeviceMgr* device_mgr, Env* env, Device* device,
    int graph_def_version, const FunctionLibraryDefinition* lib_def,
    thread::ThreadPool* thread_pool, const OptimizerOptions& optimizer_options,
    ProcessFunctionLibraryRuntime* parent);

// FunctionLibraryRuntime::GetFunctionBody returns a description of an
// instantiated function that is represented as a Graph with arg/ret
// nodes annotated.
struct FunctionBody {
  FunctionDef fdef;
  Graph* graph = nullptr;  // owned.
  DataTypeVector arg_types;
  DataTypeVector ret_types;
  gtl::InlinedVector<Node*, 4> arg_nodes;
  gtl::InlinedVector<Node*, 4> ret_nodes;
  gtl::InlinedVector<Node*, 4> control_ret_nodes;

  FunctionBody() {}
  FunctionBody(const FunctionDef& f, DataTypeSlice arg_types,
               DataTypeSlice ret_types, Graph* g);
  ~FunctionBody();
};

// Debugging facility.  Returns a debug string for a graph
// representing an instantiated function.
string DebugString(const Graph* instantiated_func_graph);

// A few hand-crafted optimization on the instantiated function body
// (a Graph*).

// Removes nodes that are
//   1. not stateful; and
//   2. not _Arg; and
//   3. not reachable from _Retval.
//
// This function is triggered by function inlining, unlike 'PruneFunctionBody'
// it doesn't preserve nodes that are reachable from control returns. Function
// inlining is responsible for connecting control return nodes with the nodes
// that have input control edges from the inlined function call node.
//
// Assuming that automatic control dependency tracking is correct, absence of
// outgoing control edge from the function call node means that no one needs to
// observe side-effect that might have been generated by the function (see
// documentation in common_runtime/function.cc for details).
//
// Returns true iff any node is removed from "g".
bool RemoveDeadNodes(Graph* g);

// Find a pattern:
//   src -(in)-> node -(out)-> dst, where
// 1) node is an identity node;
// 2) in is the only incoming data edge;
// 3) out is the only outgoing data edge;
//
// Rewrites the above pattern with src->dst and relevant data
// dependencies updated. Repeat the process until no such pattern
// left.
bool RemoveIdentityNodes(Graph* g);

// Rewrites _ListToArray and _ArrayToList to a set of Identity nodes.
bool RemoveListArrayConverter(Graph* g);

// Dump the contents of the "graph" to log files if the logging level is
// sufficiently high.
void DumpGraph(StringPiece label, const Graph* g);

// Applies graph rewrite optimization such as inlining, dead code
// removal, etc.
//
// **g is a graph constructed based on the runtime library 'lib'.
// OptimizeGraph mutates **g extensively and replaces '*g' with a
// complete copy. Therefore, the caller should not keep any references
// to nodes *g.
void OptimizeGraph(FunctionLibraryRuntime* lib, std::unique_ptr<Graph>* g,
                   const GraphOptimizer::Options& graph_optimizer_options);
void OptimizeGraph(FunctionLibraryRuntime* lib, std::unique_ptr<Graph>* g);

// Convert the Graph of a function to a GraphDef.
//
// Handles renaming of nodes to avoid duplicate names which may
// be present after various rewriting operations.
void ToGraphDef(const Graph* g, GraphDef* gdef, bool pretty = false);

// Given a numerical function "f", returns another numerical function
// "g", such that if "f" takes N inputs and produces M outputs, "g"
// takes N + M inputs and produces N outputs. I.e., if
//   (y1, y2, ..., y_M) = f(x1, x2, ..., x_N),
// g is a function which is
//   (dL/dx1, dL/dx2, ..., dL/dx_N) = g(x1, x2, ..., x_N,
//                                     dL/dy1, dL/dy2, ..., dL/dy_M),
// where L is a scalar-value function of (...x_i...).
//
// TODO(zhifengc): Asks math expert to say the comment again.
FunctionBody* SymbolicGradient(const FunctionBody& f);

struct InlineFunctionBodyOptions {
  // All nodes that have incoming control edge *from* the function call node,
  // will be forwarded to the "output control node". There are two options for
  // choosing which nodes will have a control edge *to* the "output control
  // node":
  //   a) control returns            (`control_ret` field in FunctionDef)
  //   b) data returns               (`ret` field in FunctionDef)
  enum class OutputControlSource { kDataOutputs, kControlOutputs };

  // Ignore '_noinline' function attribute.
  bool ignore_noinline = false;
  // If 'true' function inlining will override explicitly specified devices
  // inside function body with the caller node device.
  bool override_device = false;
  // If 'true' function inlining will add an IdentityN node to the graph with
  // the same name as the caller node. It will have a control edge from inlined
  // 'output_control_node' and data edges from function output nodes. IdentityN
  // node will be placed on the same device as the caller node.
  // This is mostly for compatibility with Tensorflow v1 and sessions. When we
  // prepare a graph for execution in GraphExecutionState::MakeForBaseGraph we
  // don't know what nodes will be fetched, so we can't safely remove any of
  // them. When graph executed as a function it has 'Retval' nodes for all
  // fetched tensors, and we can safely inline function calls.
  bool keep_caller_fetchable = false;
  // For compatibility with Tensorflow v1 by default we will use data outputs.
  // Control returns were added to Tensorflow v2 with automatic control
  // dependencies tracking in Eager mode.
  OutputControlSource output_control_src = OutputControlSource::kDataOutputs;

  // A human-readable debug string for this options.
  string DebugString() const;
};

// Returns 'Status::OK()' iff the function '*fbody' can be inlined at 'node'
// based on the type signature of 'node' and 'fbody':
//
// (1) Caller node has the same number of inputs and outputs as the function.
// (2) Caller node inputs and outputs have the same data types as function
//     inputs and returns.
// (3) Validation rules defined in InlineFunctionBodyOptions.
//
// If function can't be safely inlined, returns error message with details why
// inlining is not possible or safe.
Status ValidateInlining(const Node* node, const FunctionBody* fbody,
                        const InlineFunctionBodyOptions& options);

// Given a "caller" in graph "g", which is a function call of a function
// to "fbody". Replaces the "caller" with fbody->graph and connects
// edges properly. "override_device" specifies whether inlining should replace
// explicitly specified devices inside fbody with the callee's device.
//
// Returns 'Status::OK()' if function was successfully inlined into the graph.
// If function inlining is not possible returns a error with a reason, and
// leaves the graph in unmodified state.
Status InlineFunctionBody(const FunctionLibraryDefinition& flib_def, Graph* g,
                          Node* caller, const FunctionBody* fbody,
                          const InlineFunctionBodyOptions& options);

// There are three types of function calls that could be invoked during
// *Tensorflow graph execution*:
//
// 1) Native function call (node.type_string() is the function name). These
//    functions are always executed on a single-device, which is the device of
//    the function call node.
//
// 2) Multi-device function calls (PartitionedCall or StatefulPartitionedCall
//    ops) can execute on multiple devices and accept DT_RESOURCE inputs that
//    belong to different devices. This type of functions was added in
//    Tensorflow 2.0 Eager mode, and it has control outputs to represent
//    side-effects that must always execute (see `control_ret` in FunctionDef).
//
// 3) SymbolicGradient has been deprecated for a while, but we still keep it and
//    use `native` options for inlining for compatibility.
//
// We need to have distinct inlining rules for compatibility with Tensorflow v1.
//
// There are few other places in Tensorflow that could execute functions:
//
// 1) common_runtime/eager/kernel_and_device.{h,cc} - executes "top level"
//    functions directly via function library runtime, without going through
//    the graph.
// 2) tf.data pipelines - also execute functions directly via function library
//    runtime with custom executors.
struct ExpandInlineFunctionsOptions {
  ExpandInlineFunctionsOptions() : native_options(), multi_device_options() {
    using OutputControlSrc = InlineFunctionBodyOptions::OutputControlSource;
    multi_device_options.output_control_src = OutputControlSrc::kControlOutputs;
  }

  InlineFunctionBodyOptions native_options;
  InlineFunctionBodyOptions multi_device_options;
};

// WARNING(ezhulenev): PLEASE DO NOT USE THIS FUNCTION. This is a temporary
// workaround that will be enabled only during the function inlining unification
// (b/126811947). Contact ezhulenev@ if you think you need it.
// TODO(ezhulenev): Delete this function.
bool ExpandInlineFunctions(FunctionLibraryRuntime* lib, Graph* graph,
                           const ExpandInlineFunctionsOptions& options);

// For each node in "graph", if "lib" indicates that the node is a
// function call, inline the function body. Returns true if at least
// one node is inlined.
//
// This routine goes through "graph" nodes once and applies the
// inlining. The caller may decide to apply the inlining on "graph"
// multiple times by calling ExpandInlineFunctions a few times.
//
// Function calls that can't be safely inlined into the graph (ValidateInlining
// returns error), are ignored.
//
// TODO(ezhulenev): We do not FunctionLibraryRuntime for this. We need just the
// FunctionLibraryDefinition and FunctionDefToBodyHelper to implement this (see
// lower_function_call.cc).
inline bool ExpandInlineFunctions(FunctionLibraryRuntime* lib, Graph* graph) {
  return ExpandInlineFunctions(lib, graph, ExpandInlineFunctionsOptions());
}

// Extracts function name and attributes from `call_def`
// `call_def` can be a native function call (where the op type is the function
// name) or a call through PartitionedCall/StatefulPartitionedCall.
Status NameAndAttrsFromFunctionCall(const NodeDef& call_def,
                                    NameAttrList* function);

// Extracts function name and attributes from `call_def` and invokes
// flr->Instantiate(name, attrs, handle).
// `call_def` can be a native function call (where the op type is the function
// name) or a call through PartitionedCall/StatefulPartitionedCall.
Status InstantiateFunctionCall(const NodeDef& call_def,
                               FunctionLibraryRuntime& flr,
                               FunctionLibraryRuntime::Handle* handle);

// Returns true iff `n` represents a function call. `n` can be a native
// function call (n.type_string() is the function name),
// a PartitionedCall/StatefulPartitionedCall, or a SymbolicGradient (which
// has been deprecated for a while).
bool IsFunctionCall(const FunctionLibraryDefinition& lib_def, const Node& n);

// Instantiates FunctionDef into a graph. Set *fbody to point to the
// FunctionBody that holds the instantiated FunctionDef.
Status FunctionDefToBodyHelper(
    const FunctionDef& fdef, const AttrSlice& attrs,
    const FunctionLibraryDefinition* const lib_def,
    const std::function<Status(const string&, const OpDef**)>& get_func_sig,
    FunctionBody** fbody);
}  // end namespace tensorflow

#endif  // TENSORFLOW_CORE_COMMON_RUNTIME_FUNCTION_H_
