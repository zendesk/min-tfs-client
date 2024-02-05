/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

// See docs in ../ops/math_ops.cc.
#include "tensorflow/core/kernels/segment_reduction_ops_impl.h"

namespace tensorflow {

#define REGISTER_CPU_KERNEL_SEGMENT(name, functor, type, index_type, \
                                    default_value)                   \
  REGISTER_KERNEL_BUILDER(                                           \
      Name(name)                                                     \
          .Device(DEVICE_CPU)                                        \
          .TypeConstraint<type>("T")                                 \
          .TypeConstraint<index_type>("Tindices"),                   \
      SegmentReductionOp<CPUDevice, type, index_type, functor, default_value>)

#define REGISTER_REAL_CPU_KERNELS(type, index_type)                            \
  REGISTER_CPU_KERNEL_SEGMENT("SegmentSum", Eigen::internal::SumReducer<type>, \
                              type, index_type, 0);                            \
  REGISTER_CPU_KERNEL_SEGMENT(                                                 \
      "SegmentMean", Eigen::internal::MeanReducer<type>, type, index_type, 0); \
  REGISTER_CPU_KERNEL_SEGMENT(                                                 \
      "SegmentProd", Eigen::internal::ProdReducer<type>, type, index_type, 1); \
  REGISTER_CPU_KERNEL_SEGMENT("SegmentMin", Eigen::internal::MinReducer<type>, \
                              type, index_type, 0);                            \
  REGISTER_CPU_KERNEL_SEGMENT("SegmentMax", Eigen::internal::MaxReducer<type>, \
                              type, index_type, 0)

#define REGISTER_COMPLEX_CPU_KERNELS(type, index_type)                         \
  REGISTER_CPU_KERNEL_SEGMENT("SegmentSum", Eigen::internal::SumReducer<type>, \
                              type, index_type, 0);                            \
  REGISTER_CPU_KERNEL_SEGMENT(                                                 \
      "SegmentMean", Eigen::internal::MeanReducer<type>, type, index_type, 0); \
  REGISTER_CPU_KERNEL_SEGMENT(                                                 \
      "SegmentProd", Eigen::internal::ProdReducer<type>, type, index_type, 1);

#define REGISTER_REAL_CPU_KERNELS_ALL(type) \
  REGISTER_REAL_CPU_KERNELS(type, int64_t)

#define REGISTER_COMPLEX_CPU_KERNELS_ALL(type) \
  REGISTER_COMPLEX_CPU_KERNELS(type, int64_t)

TF_CALL_REAL_NUMBER_TYPES(REGISTER_REAL_CPU_KERNELS_ALL);
REGISTER_COMPLEX_CPU_KERNELS_ALL(complex64);
REGISTER_COMPLEX_CPU_KERNELS_ALL(complex128);
#undef REGISTER_CPU_KERNEL_SEGMENT
#undef REGISTER_REAL_CPU_KERNELS
#undef REGISTER_COMPLEX_CPU_KERNELS
#undef REGISTER_REAL_CPU_KERNELS_ALL
#undef REGISTER_COMPLEX_CPU_KERNELS_ALL

#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM
#define REGISTER_GPU_KERNEL_SORTEDSEGMENT(                            \
    name, type, index_type, initial_value_functor,                    \
    empty_segment_value_functor, reduction_kernel_functor, is_mean)   \
  REGISTER_KERNEL_BUILDER(                                            \
      Name(name)                                                      \
          .Device(DEVICE_GPU)                                         \
          .TypeConstraint<type>("T")                                  \
          .TypeConstraint<index_type>("Tindices"),                    \
      SegmentReductionGPUOp<                                          \
          type, index_type,                                           \
          functor::SegmentReductionFunctor<                           \
              type, index_type, initial_value_functor,                \
              empty_segment_value_functor, reduction_kernel_functor>, \
          is_mean>)

#define REGISTER_GPU_SORTED_KERNELS(type, index_type)                         \
  REGISTER_GPU_KERNEL_SORTEDSEGMENT("SegmentSum", type, index_type,           \
                                    functor::Zero<type>, functor::Zero<type>, \
                                    functor::Sum, /*is_mean=*/false);         \
  REGISTER_GPU_KERNEL_SORTEDSEGMENT("SegmentMean", type, index_type,          \
                                    functor::Zero<type>, functor::Zero<type>, \
                                    functor::Sum, /*is_mean=*/true);          \
  REGISTER_GPU_KERNEL_SORTEDSEGMENT("SegmentProd", type, index_type,          \
                                    functor::One<type>, functor::One<type>,   \
                                    functor::Prod, /*is_mean=*/false);        \
  REGISTER_GPU_KERNEL_SORTEDSEGMENT(                                          \
      "SegmentMin", type, index_type, functor::Highest<type>,                 \
      functor::Zero<type>, functor::Min, /*is_mean=*/false);                  \
  REGISTER_GPU_KERNEL_SORTEDSEGMENT(                                          \
      "SegmentMax", type, index_type, functor::Lowest<type>,                  \
      functor::Zero<type>, functor::Max, /*is_mean=*/false);

#define REGISTER_GPU_SORTED_KERNELS_ALL(type) \
  REGISTER_GPU_SORTED_KERNELS(type, int64_t);

TF_CALL_GPU_NUMBER_TYPES(REGISTER_GPU_SORTED_KERNELS_ALL);
#undef REGISTER_GPU_KERNEL_SORTEDSEGMENT
#undef REGISTER_GPU_SORTED_KERNELS
#undef REGISTER_GPU_SORTED_KERNELS_ALL
#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM

}  // namespace tensorflow
