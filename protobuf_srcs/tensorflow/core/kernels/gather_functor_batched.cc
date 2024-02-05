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

#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM

#include "tensorflow/core/kernels/gather_functor_batched.h"

#include "tensorflow/core/framework/register_types.h"

namespace tensorflow {

typedef Eigen::GpuDevice GPUDevice;

namespace functor {

// Forward declarations of the functor specializations for GPU.
#define DECLARE_GPU_SPECS_INDEX(T, Index)                               \
  template <>                                                           \
  int64_t GatherFunctorBatched<GPUDevice, T, Index>::operator()(        \
      OpKernelContext* ctx, typename TTypes<T, 4>::ConstTensor Tparams, \
      typename TTypes<Index>::ConstFlat Tindices,                       \
      typename TTypes<T, 4>::Tensor Tout);                              \
  extern template struct GatherFunctorBatched<GPUDevice, T, Index>;

#define DECLARE_GPU_SPECS(T)         \
  DECLARE_GPU_SPECS_INDEX(T, int32); \
  DECLARE_GPU_SPECS_INDEX(T, int64_t)

TF_CALL_int64(DECLARE_GPU_SPECS);
TF_CALL_GPU_NUMBER_TYPES(DECLARE_GPU_SPECS);
TF_CALL_COMPLEX_TYPES(DECLARE_GPU_SPECS);

#undef DECLARE_GPU_SPECS
#undef DECLARE_GPU_SPECS_INDEX

}  // namespace functor
}  // namespace tensorflow

#else

#include "tensorflow/core/kernels/gather_functor_batched.h"

#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM
