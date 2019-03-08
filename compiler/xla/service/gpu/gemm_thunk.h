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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_GPU_GEMM_THUNK_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_GPU_GEMM_THUNK_H_

#include "tensorflow/compiler/xla/service/buffer_assignment.h"
#include "tensorflow/compiler/xla/service/gpu/buffer_allocations.h"
#include "tensorflow/compiler/xla/service/gpu/gpu_executable.h"
#include "tensorflow/compiler/xla/service/gpu/hlo_execution_profiler.h"
#include "tensorflow/compiler/xla/service/gpu/thunk.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/platform/stream_executor_no_cuda.h"

namespace xla {
namespace gpu {

// This class stores everything that StreamExecutor needs to launch a BLAS gemm.
// It is generated by IrEmitter.
//
// This is thread-compatible.
class GemmThunk : public Thunk {
 public:
  // Constructs a thunk that computes "output = (lhs <dot> rhs) * alpha" using
  // BLAS gemm.  hlo_instruction is as in Thunk. alpha is a constant.
  GemmThunk(const BufferAllocation::Slice& lhs_buffer,
            const BufferAllocation::Slice& rhs_buffer,
            const BufferAllocation::Slice& output_buffer,
            const Shape& lhs_shape, const Shape& rhs_shape,
            const Shape& output_shape, double alpha, double beta,
            const HloInstruction* hlo_instruction,
            bool implements_whole_instruction);

  GemmThunk(const GemmThunk&) = delete;
  GemmThunk& operator=(const GemmThunk&) = delete;

  // Does the gemm operation for the thunk on "stream", which must be non-null.
  Status ExecuteOnStream(const BufferAllocations& buffer_allocations,
                         se::Stream* stream,
                         HloExecutionProfiler* profiler) override;

  bool WillAutotuneKernel(se::Stream* stream) override {
    // We will autotune this kernel if we don't already have a autotune result
    // for the stream device.
    return autotune_results_.find(
               stream->parent()->GetDeviceDescription().name()) ==
           autotune_results_.end();
  }

 private:
  const BufferAllocation::Slice lhs_buffer_;
  const BufferAllocation::Slice rhs_buffer_;
  const BufferAllocation::Slice output_buffer_;

  const Shape lhs_shape_;
  const Shape rhs_shape_;
  const Shape output_shape_;

  const double alpha_;
  const double beta_;

  const bool implements_whole_instruction_;

  // Maps device names (StreamExecutor::DeviceDescription::name()) to autotune
  // results.  The map's value is the best algorithm we've found for this thunk
  // on this device, or an error if none of the algorithms worked and we should
  // use the regular gemm without an algorithm.
  //
  // TODO(b/112415150):  Make this thread safe.
  std::unordered_map<string, StatusOr<se::blas::AlgorithmType>>
      autotune_results_;
};

}  // namespace gpu
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_GPU_GEMM_THUNK_H_
