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

#define EIGEN_USE_THREADS

#include "tensorflow/core/kernels/nextafter_op.h"
#include "tensorflow/core/kernels/cwise_ops_common.h"

namespace tensorflow {

REGISTER2(BinaryOp, CPU, "NextAfter", functor::nextafter, float, double);

#if TENSORFLOW_USE_SYCL
#define REGISTER_SYCL_KERNEL(TYPE)                                     \
  REGISTER_KERNEL_BUILDER(                                             \
      Name("NextAfter").Device(DEVICE_SYCL).TypeConstraint<TYPE>("T"), \
      BinaryOp<SYCLDevice, functor::nextafter<TYPE>>);
REGISTER_SYCL_KERNEL(float);
REGISTER_SYCL_KERNEL(double);
#undef REGISTER_SYCL_KERNEL
#endif  // TENSORFLOW_USE_SYCL

#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM
REGISTER2(BinaryOp, GPU, "NextAfter", functor::nextafter, float, double);
#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM

}  // namespace tensorflow
