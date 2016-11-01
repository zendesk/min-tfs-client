/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

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

#if !TENSORFLOW_USE_SYCL
#error This file must only be included when building TensorFlow with SYCL support
#endif

#ifndef TENSORFLOW_CORE_KERNELS_CWISE_OPS_SYCL_COMMON_H_
#define TENSORFLOW_CORE_KERNELS_CWISE_OPS_SYCL_COMMON_H_

#define EIGEN_USE_SYCL

#include "tensorflow/core/framework/tensor_types.h"
#include "tensorflow/core/kernels/cwise_ops.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {
namespace functor {

typedef Eigen::SyclDevice SYCLDevice;

template <typename OUT, typename RHS>
void Assign(const SYCLDevice& d, OUT out, RHS rhs) {
  out.device(d) = rhs;
}

// Partial specialization of UnaryFunctor<Device=SYCLDevice, Functor>.
template <typename Functor>
struct UnaryFunctor<SYCLDevice, Functor> {
  void operator()(const SYCLDevice& d, typename Functor::tout_type out,
                  typename Functor::tin_type in) {
    To32Bit(out).device(d) = To32Bit(in).unaryExpr(typename Functor::func());
  }
};

// Partial specialization of BinaryFunctor<Device=SYCLDevice, Functor>.
template <typename Functor, int NDIMS, bool has_errors>
struct BinaryFunctor<SYCLDevice, Functor, NDIMS, has_errors> {
  void operator()(const SYCLDevice& d, typename Functor::tout_type out,
                  typename Functor::tin_type in0,
                  typename Functor::tin_type in1, bool* error) {
    Assign(d, out, in0.binaryExpr(in1, typename Functor::func()));
  }

  void Left(const SYCLDevice& d, typename Functor::tout_type out,
            typename Functor::tscalar_type scalar,
            typename Functor::tin_type in, bool* error) {
    LOG(FATAL) << "BinaryFunctor::Left NOT IMPLEMENTED ! ";
  }

  void Right(const SYCLDevice& d, typename Functor::tout_type out,
             typename Functor::tin_type in,
             typename Functor::tscalar_type scalar, bool* error) {
    typedef typename Functor::out_type Tout;
    typedef typename Functor::in_type Tin;
    typedef typename Functor::func Binary;
    typedef typename Eigen::internal::scalar_right<Tout, Tin, Binary> Unary;
    Assign(d, out, in.unaryExpr(Unary(scalar.data())));
  }

  void BCast(const SYCLDevice& d,
             typename TTypes<typename Functor::out_type, NDIMS>::Tensor out,
             typename TTypes<typename Functor::in_type, NDIMS>::ConstTensor in0,
             typename Eigen::array<Eigen::DenseIndex, NDIMS> bcast0,
             typename TTypes<typename Functor::in_type, NDIMS>::ConstTensor in1,
             typename Eigen::array<Eigen::DenseIndex, NDIMS> bcast1,
             bool* error) {
    LOG(FATAL) << "BinaryFunctor::BCast NOT IMPLEMENTED ";
  }
};

// Macros to explicitly instantiate kernels on GPU for multiple types
// (T0, T1, etc.) for UnaryFunctor (e.g., functor::sqrt).
#define DEFINE_UNARY1(F, T) template struct UnaryFunctor<SYCLDevice, F<T> >
#define DEFINE_UNARY2(F, T0, T1) \
  DEFINE_UNARY1(F, T0);          \
  DEFINE_UNARY1(F, T1)
#define DEFINE_UNARY3(F, T0, T1, T2) \
  DEFINE_UNARY2(F, T0, T1);          \
  DEFINE_UNARY1(F, T2)
#define DEFINE_UNARY4(F, T0, T1, T2, T3) \
  DEFINE_UNARY2(F, T0, T1);              \
  DEFINE_UNARY2(F, T2, T3)
#define DEFINE_UNARY5(F, T0, T1, T2, T3, T4) \
  DEFINE_UNARY2(F, T0, T1);                  \
  DEFINE_UNARY3(F, T2, T3, T4)

// Macros to explicitly instantiate kernels on GPU for multiple types
// (T0, T1, etc.) for BinaryFunctor.
#define DEFINE_BINARY1(F, T)                          \
  template struct BinaryFunctor<SYCLDevice, F<T>, 1>; \
  template struct BinaryFunctor<SYCLDevice, F<T>, 2>; \
  template struct BinaryFunctor<SYCLDevice, F<T>, 3>
#define DEFINE_BINARY2(F, T0, T1) \
  DEFINE_BINARY1(F, T0);          \
  DEFINE_BINARY1(F, T1)
#define DEFINE_BINARY3(F, T0, T1, T2) \
  DEFINE_BINARY2(F, T0, T1);          \
  DEFINE_BINARY1(F, T2)
#define DEFINE_BINARY4(F, T0, T1, T2, T3) \
  DEFINE_BINARY2(F, T0, T1);              \
  DEFINE_BINARY2(F, T2, T3)
#define DEFINE_BINARY5(F, T0, T1, T2, T3, T4) \
  DEFINE_BINARY2(F, T0, T1);                  \
  DEFINE_BINARY3(F, T2, T3, T4)
#define DEFINE_BINARY6(F, T0, T1, T2, T3, T4, T5) \
  DEFINE_BINARY3(F, T0, T1, T2);                  \
  DEFINE_BINARY3(F, T3, T4, T5)
#define DEFINE_BINARY7(F, T0, T1, T2, T3, T4, T5, T6) \
  DEFINE_BINARY3(F, T0, T1, T2);                      \
  DEFINE_BINARY4(F, T3, T4, T5, T6)
#define DEFINE_BINARY8(F, T0, T1, T2, T3, T4, T5, T6, T7) \
  DEFINE_BINARY4(F, T0, T1, T2, T3);                      \
  DEFINE_BINARY4(F, T4, T5, T6, T7)
#define DEFINE_BINARY9(F, T0, T1, T2, T3, T4, T5, T6, T7, T8) \
  DEFINE_BINARY4(F, T0, T1, T2, T3);                          \
  DEFINE_BINARY5(F, T4, T5, T6, T7, T8)
#define DEFINE_BINARY10(F, T0, T1, T2, T3, T4, T5, T6, T7, T8, T9) \
  DEFINE_BINARY5(F, T0, T1, T2, T3, T4);                           \
  DEFINE_BINARY5(F, T5, T6, T7, T8, T9)

}  // end namespace functor
}  // end namespace tensorflow

#endif  // TENSORFLOW_CORE_KERNELS_CWISE_OPS_SYCL_COMMON_H_
