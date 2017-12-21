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
#ifndef THIRD_PARTY_TENSORFLOW_CONTRIB_LITE_KERNELS_INTERNAL_REFERENCE_REFERENCE_OPS_H_
#define THIRD_PARTY_TENSORFLOW_CONTRIB_LITE_KERNELS_INTERNAL_REFERENCE_REFERENCE_OPS_H_

#include <stdint.h>
#include <sys/types.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <type_traits>

#include "third_party/eigen3/Eigen/Core"
#include "fixedpoint/fixedpoint.h"
#include "public/gemmlowp.h"
#include "tensorflow/contrib/lite/kernels/internal/common.h"
#include "tensorflow/contrib/lite/kernels/internal/round.h"
#include "tensorflow/contrib/lite/kernels/internal/types.h"

namespace tflite {
namespace reference_ops {

inline int32 MultiplyByQuantizedMultiplierSmallerThanOne(
    int32 x, int32 quantized_multiplier, int right_shift) {
  using gemmlowp::RoundingDivideByPOT;
  using gemmlowp::SaturatingRoundingDoublingHighMul;
  return RoundingDivideByPOT(
      SaturatingRoundingDoublingHighMul(x, quantized_multiplier), right_shift);
}

inline int32 MultiplyByQuantizedMultiplierGreaterThanOne(
    int32 x, int32 quantized_multiplier, int left_shift) {
  using gemmlowp::SaturatingRoundingDoublingHighMul;
  return SaturatingRoundingDoublingHighMul(x * (1 << left_shift),
                                           quantized_multiplier);
}

template <typename T>
int CountLeadingZeros(T integer_input) {
  static_assert(std::is_unsigned<T>::value,
                "Only unsigned integer types handled.");
  const T one_in_leading_positive = static_cast<T>(1)
                                    << (std::numeric_limits<T>::digits - 1);
  int leading_zeros = 0;
  while (integer_input < one_in_leading_positive) {
    integer_input <<= 1;
    ++leading_zeros;
  }
  return leading_zeros;
}

// DO NOT USE THIS STRUCT FOR NEW FUNCTIONALITY BEYOND IMPLEMENTING ELEMENT-WISE
// BROADCASTING.
//
// NdArrayDesc<N> describes the shape and memory layout of an N-dimensional
// rectangular array of numbers.
//
// NdArrayDesc<N> is basically identical to Dims<N> defined in types.h.
// However, as Dims<N> is to be deprecated, this class exists as an adaptor
// to enable simple unoptimized implementations of element-wise broadcasting
// operations.
template <int N>
struct NdArrayDesc {
  // The "extent" of each dimension. Indices along dimension d must be in the
  // half-open interval [0, extents[d]).
  int extents[N];

  // The number of *elements* (not bytes) between consecutive indices of each
  // dimension.
  int strides[N];
};

// DO NOT USE THIS FUNCTION FOR NEW FUNCTIONALITY BEYOND IMPLEMENTING
// ELEMENT-WISE BROADCASTING.
//
// Same as Offset(), except takes as NdArrayDesc<N> instead of Dims<N>.
inline int SubscriptToIndex(const NdArrayDesc<4>& desc, int i0, int i1, int i2,
                            int i3) {
  TFLITE_DCHECK(i0 >= 0 && i0 < desc.extents[0]);
  TFLITE_DCHECK(i1 >= 0 && i1 < desc.extents[1]);
  TFLITE_DCHECK(i2 >= 0 && i2 < desc.extents[2]);
  TFLITE_DCHECK(i3 >= 0 && i3 < desc.extents[3]);
  return i0 * desc.strides[0] + i1 * desc.strides[1] + i2 * desc.strides[2] +
         i3 * desc.strides[3];
}

// Given the dimensions of the operands for an element-wise binary broadcast,
// adjusts them so that they can be directly iterated over with simple loops.
// Returns the adjusted dims as instances of NdArrayDesc in 'desc0_out' and
// 'desc1_out'. 'desc0_out' and 'desc1_out' cannot be nullptr.
//
// This function assumes that the two input shapes are compatible up to
// broadcasting and the shorter one has already been prepended with 1s to be the
// same length. E.g., if shape0 is (1, 16, 16, 64) and shape1 is (1, 64),
// shape1 must already have been prepended to be (1, 1, 1, 64). Recall that
// Dims<N> refer to shapes in reverse order. In this case, input0_dims will be
// (64, 16, 16, 1) and input1_dims will be (64, 1, 1, 1).
//
// When two shapes are compatible up to broadcasting, for each dimension d,
// the input extents are either equal, or one of them is 1.
//
// This function performs the following for each dimension d:
// - If the extents are equal, then do nothing since the loop that walks over
//   both of the input arrays is correct.
// - Otherwise, one (and only one) of the extents must be 1. Say extent0 is 1
//   and extent1 is e1. Then set extent0 to e1 and stride0 *to 0*. This allows
//   array0 to be referenced *at any index* in dimension d and still access the
//   same slice.
template <int N>
inline void NdArrayDescsForElementwiseBroadcast(const Dims<N>& input0_dims,
                                                const Dims<N>& input1_dims,
                                                NdArrayDesc<N>* desc0_out,
                                                NdArrayDesc<N>* desc1_out) {
  TFLITE_DCHECK(desc0_out != nullptr);
  TFLITE_DCHECK(desc1_out != nullptr);

  // Copy dims to desc.
  for (int i = 0; i < N; ++i) {
    desc0_out->extents[i] = input0_dims.sizes[i];
    desc0_out->strides[i] = input0_dims.strides[i];
    desc1_out->extents[i] = input1_dims.sizes[i];
    desc1_out->strides[i] = input1_dims.strides[i];
  }

  // Walk over each dimension. If the extents are equal do nothing.
  // Otherwise, set the desc with extent 1 to have extent equal to the other and
  // stride 0.
  for (int i = 0; i < N; ++i) {
    const int extent0 = ArraySize(input0_dims, i);
    const int extent1 = ArraySize(input1_dims, i);
    if (extent0 != extent1) {
      if (extent0 == 1) {
        desc0_out->strides[i] = 0;
        desc0_out->extents[i] = extent1;
      } else {
        TFLITE_DCHECK_EQ(extent1, 1);
        desc1_out->strides[i] = 0;
        desc1_out->extents[i] = extent0;
      }
    }
  }
}

inline void Conv(const float* input_data, const Dims<4>& input_dims,
                 const float* filter_data, const Dims<4>& filter_dims,
                 const float* bias_data, const Dims<4>& bias_dims,
                 int stride_width, int stride_height, int pad_width,
                 int pad_height, float output_activation_min,
                 float output_activation_max, float* output_data,
                 const Dims<4>& output_dims, float* im2col_data,
                 const Dims<4>& im2col_dims) {
  (void)im2col_data;  // only used in optimized code.
  (void)im2col_dims;  // only used in optimized code.
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int input_depth = MatchingArraySize(input_dims, 0, filter_dims, 0);
  const int output_depth = MatchingArraySize(filter_dims, 3, output_dims, 0);
  if (bias_data) {
    TFLITE_DCHECK_EQ(ArraySize(filter_dims, 3), ArraySize(bias_dims, 0));
  }
  const int input_height = ArraySize(input_dims, 2);
  const int input_width = ArraySize(input_dims, 1);
  const int filter_height = ArraySize(filter_dims, 2);
  const int filter_width = ArraySize(filter_dims, 1);
  const int output_height = ArraySize(output_dims, 2);
  const int output_width = ArraySize(output_dims, 1);
  for (int batch = 0; batch < batches; ++batch) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      for (int out_x = 0; out_x < output_width; ++out_x) {
        for (int out_channel = 0; out_channel < output_depth; ++out_channel) {
          const int in_x_origin = (out_x * stride_width) - pad_width;
          const int in_y_origin = (out_y * stride_height) - pad_height;
          float total = 0.f;
          for (int filter_y = 0; filter_y < filter_height; ++filter_y) {
            for (int filter_x = 0; filter_x < filter_width; ++filter_x) {
              for (int in_channel = 0; in_channel < input_depth; ++in_channel) {
                const int in_x = in_x_origin + filter_x;
                const int in_y = in_y_origin + filter_y;
                // If the location is outside the bounds of the input image,
                // use zero as a default value.
                if ((in_x >= 0) && (in_x < input_width) && (in_y >= 0) &&
                    (in_y < input_height)) {
                  float input_value = input_data[Offset(input_dims, in_channel,
                                                        in_x, in_y, batch)];
                  float filter_value =
                      filter_data[Offset(filter_dims, in_channel, filter_x,
                                         filter_y, out_channel)];
                  total += (input_value * filter_value);
                }
              }
            }
          }
          float bias_value = 0.0f;
          if (bias_data) {
            bias_value = bias_data[Offset(bias_dims, out_channel, 0, 0, 0)];
          }
          output_data[Offset(output_dims, out_channel, out_x, out_y, batch)] =
              ActivationFunctionWithMinMax(total + bias_value,
                                           output_activation_min,
                                           output_activation_max);
        }
      }
    }
  }
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void Conv(const float* input_data, const Dims<4>& input_dims,
          const float* filter_data, const Dims<4>& filter_dims,
          const float* bias_data, const Dims<4>& bias_dims, int stride_width,
          int stride_height, int pad_width, int pad_height, float* output_data,
          const Dims<4>& output_dims, float* im2col_data,
          const Dims<4>& im2col_dims) {
  float output_activation_min, output_activation_max;
  GetActivationMinMax(Ac, &output_activation_min, &output_activation_max);
  Conv(input_data, input_dims, filter_data, filter_dims, bias_data, bias_dims,
       stride_width, stride_height, pad_width, pad_height,
       output_activation_min, output_activation_max, output_data, output_dims,
       im2col_data, im2col_dims);
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void Conv(const float* input_data, const Dims<4>& input_dims,
          const float* filter_data, const Dims<4>& filter_dims,
          const float* bias_data, const Dims<4>& bias_dims, int stride,
          int pad_width, int pad_height, float* output_data,
          const Dims<4>& output_dims, float* im2col_data,
          const Dims<4>& im2col_dims) {
  Conv<Ac>(input_data, input_dims, filter_data, filter_dims, bias_data,
           bias_dims, stride, stride, pad_width, pad_height, output_data,
           output_dims, im2col_data, im2col_dims);
}

inline void Conv(const uint8* input_data, const Dims<4>& input_dims,
                 int32 input_offset, const uint8* filter_data,
                 const Dims<4>& filter_dims, int32 filter_offset,
                 const int32* bias_data, const Dims<4>& bias_dims,
                 int stride_width, int stride_height, int pad_width,
                 int pad_height, int32 output_offset, int32 output_multiplier,
                 int output_shift, int32 output_activation_min,
                 int32 output_activation_max, uint8* output_data,
                 const Dims<4>& output_dims, uint8* im2col_data,
                 const Dims<4>& im2col_dims,
                 gemmlowp::GemmContext* gemm_context) {
  (void)im2col_data;   // only used in optimized code.
  (void)im2col_dims;   // only used in optimized code.
  (void)gemm_context;  // only used in optimized code.
  TFLITE_DCHECK_LE(output_activation_min, output_activation_max);
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int input_depth = MatchingArraySize(input_dims, 0, filter_dims, 0);
  const int output_depth =
      MatchingArraySize(filter_dims, 3, bias_dims, 0, output_dims, 0);
  const int input_height = ArraySize(input_dims, 2);
  const int input_width = ArraySize(input_dims, 1);
  const int filter_height = ArraySize(filter_dims, 2);
  const int filter_width = ArraySize(filter_dims, 1);
  const int output_height = ArraySize(output_dims, 2);
  const int output_width = ArraySize(output_dims, 1);
  for (int batch = 0; batch < batches; ++batch) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      for (int out_x = 0; out_x < output_width; ++out_x) {
        for (int out_channel = 0; out_channel < output_depth; ++out_channel) {
          const int in_x_origin = (out_x * stride_width) - pad_width;
          const int in_y_origin = (out_y * stride_height) - pad_height;
          int32 acc = 0;
          for (int filter_y = 0; filter_y < filter_height; ++filter_y) {
            for (int filter_x = 0; filter_x < filter_width; ++filter_x) {
              for (int in_channel = 0; in_channel < input_depth; ++in_channel) {
                const int in_x = in_x_origin + filter_x;
                const int in_y = in_y_origin + filter_y;
                // If the location is outside the bounds of the input image,
                // use zero as a default value.
                if ((in_x >= 0) && (in_x < input_width) && (in_y >= 0) &&
                    (in_y < input_height)) {
                  int32 input_val = input_data[Offset(input_dims, in_channel,
                                                      in_x, in_y, batch)];
                  int32 filter_val =
                      filter_data[Offset(filter_dims, in_channel, filter_x,
                                         filter_y, out_channel)];
                  acc +=
                      (filter_val + filter_offset) * (input_val + input_offset);
                }
              }
            }
          }
          if (bias_data) {
            acc += bias_data[Offset(bias_dims, out_channel, 0, 0, 0)];
          }
          acc = MultiplyByQuantizedMultiplierSmallerThanOne(
              acc, output_multiplier, output_shift);
          acc += output_offset;
          acc = std::max(acc, output_activation_min);
          acc = std::min(acc, output_activation_max);
          output_data[Offset(output_dims, out_channel, out_x, out_y, batch)] =
              static_cast<uint8>(acc);
        }
      }
    }
  }
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
inline void Conv(const uint8* input_data, const Dims<4>& input_dims,
                 int32 input_offset, const uint8* filter_data,
                 const Dims<4>& filter_dims, int32 filter_offset,
                 const int32* bias_data, const Dims<4>& bias_dims,
                 int stride_width, int stride_height, int pad_width,
                 int pad_height, int32 output_offset, int32 output_multiplier,
                 int output_shift, int32 output_activation_min,
                 int32 output_activation_max, uint8* output_data,
                 const Dims<4>& output_dims, uint8* im2col_data,
                 const Dims<4>& im2col_dims,
                 gemmlowp::GemmContext* gemm_context) {
  static_assert(Ac == FusedActivationFunctionType::kNone ||
                    Ac == FusedActivationFunctionType::kRelu ||
                    Ac == FusedActivationFunctionType::kRelu6 ||
                    Ac == FusedActivationFunctionType::kRelu1,
                "");
  if (Ac == FusedActivationFunctionType::kNone) {
    TFLITE_DCHECK_EQ(output_activation_min, 0);
    TFLITE_DCHECK_EQ(output_activation_max, 255);
  }
  Conv(input_data, input_dims, input_offset, filter_data, filter_dims,
       filter_offset, bias_data, bias_dims, stride_width, stride_height,
       pad_width, pad_height, output_offset, output_multiplier, output_shift,
       output_activation_min, output_activation_max, output_data, output_dims,
       im2col_data, im2col_dims, gemm_context);
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void Conv(const uint8* input_data, const Dims<4>& input_dims,
          int32 input_offset, const uint8* filter_data,
          const Dims<4>& filter_dims, int32 filter_offset,
          const int32* bias_data, const Dims<4>& bias_dims, int stride,
          int pad_width, int pad_height, int32 output_offset,
          int32 output_multiplier, int output_shift,
          int32 output_activation_min, int32 output_activation_max,
          uint8* output_data, const Dims<4>& output_dims, uint8* im2col_data,
          const Dims<4>& im2col_dims, gemmlowp::GemmContext* gemm_context) {
  Conv<Ac>(input_data, input_dims, input_offset, filter_data, filter_dims,
           filter_offset, bias_data, bias_dims, stride, stride, pad_width,
           pad_height, output_offset, output_multiplier, output_shift,
           output_activation_min, output_activation_max, output_data,
           output_dims, im2col_data, im2col_dims, gemm_context);
}

template <typename T>
inline void DepthToSpace(const T* input_data, const Dims<4>& input_dims,
                         int block_size, T* output_data,
                         const Dims<4>& output_dims) {
  const int input_depth = ArraySize(input_dims, 0);
  const int input_width = ArraySize(input_dims, 1);
  const int input_height = ArraySize(input_dims, 2);
  const int input_batch = ArraySize(input_dims, 3);

  const int output_depth = ArraySize(output_dims, 0);
  const int output_width = ArraySize(output_dims, 1);
  const int output_height = ArraySize(output_dims, 2);
  const int output_batch = ArraySize(output_dims, 3);

  TFLITE_DCHECK_EQ(input_width * block_size, output_width);
  TFLITE_DCHECK_EQ(input_height * block_size, output_height);
  TFLITE_DCHECK_EQ(input_depth, output_depth * block_size * block_size);
  TFLITE_DCHECK_EQ(input_batch, output_batch);

  for (int out_b = 0; out_b < output_batch; ++out_b) {
    for (int out_h = 0; out_h < output_height; ++out_h) {
      for (int out_w = 0; out_w < output_width; ++out_w) {
        for (int out_d = 0; out_d < output_depth; ++out_d) {
          const int in_d =
              out_d + ((out_h % block_size) * block_size + out_w % block_size) *
                          output_depth;
          const int in_w = out_w / block_size;
          const int in_h = out_h / block_size;
          const int in_b = out_b;

          const int output_index =
              Offset(output_dims, out_d, out_w, out_h, out_b);
          const int input_index = Offset(input_dims, in_d, in_w, in_h, in_b);

          output_data[output_index] = input_data[input_index];
        }
      }
    }
  }
}

template <typename T>
inline void SpaceToDepth(const T* input_data, const Dims<4>& input_dims,
                         int block_size, T* output_data,
                         const Dims<4>& output_dims) {
  const int input_depth = ArraySize(input_dims, 0);
  const int input_width = ArraySize(input_dims, 1);
  const int input_height = ArraySize(input_dims, 2);
  const int input_batch = ArraySize(input_dims, 3);

  const int output_depth = ArraySize(output_dims, 0);
  const int output_width = ArraySize(output_dims, 1);
  const int output_height = ArraySize(output_dims, 2);
  const int output_batch = ArraySize(output_dims, 3);

  TFLITE_DCHECK_EQ(input_width, output_width * block_size);
  TFLITE_DCHECK_EQ(input_height, output_height * block_size);
  TFLITE_DCHECK_EQ(input_depth * block_size * block_size, output_depth);
  TFLITE_DCHECK_EQ(input_batch, output_batch);

  for (int in_b = 0; in_b < input_batch; ++in_b) {
    for (int in_h = 0; in_h < input_height; ++in_h) {
      for (int in_w = 0; in_w < input_width; ++in_w) {
        for (int in_d = 0; in_d < input_depth; ++in_d) {
          const int out_d =
              in_d + ((in_h % block_size) * block_size + in_w % block_size) *
                         input_depth;
          const int out_w = in_w / block_size;
          const int out_h = in_h / block_size;
          const int out_b = in_b;

          const int output_index =
              Offset(output_dims, out_d, out_w, out_h, out_b);
          const int input_index = Offset(input_dims, in_d, in_w, in_h, in_b);

          output_data[output_index] = input_data[input_index];
        }
      }
    }
  }
}

inline void FullyConnected(const float* input_data, const Dims<4>& input_dims,
                           const float* weights_data,
                           const Dims<4>& weights_dims, const float* bias_data,
                           const Dims<4>& bias_dims,
                           float output_activation_min,
                           float output_activation_max, float* output_data,
                           const Dims<4>& output_dims) {
  // TODO(benoitjacob): This really should be:
  //     const int batches = ArraySize(output_dims, 1);
  // but the current --variable_batch hack consists in overwriting the 3rd
  // dimension with the runtime batch size, as we don't keep track for each
  // array of which dimension is the batch dimension in it.
  const int batches = ArraySize(output_dims, 1) * ArraySize(output_dims, 2) *
                      ArraySize(output_dims, 3);
  const int output_depth = MatchingArraySize(weights_dims, 1, output_dims, 0);
  const int accum_depth = ArraySize(weights_dims, 0);
  TFLITE_DCHECK(IsPackedWithoutStrides(input_dims));
  TFLITE_DCHECK(IsPackedWithoutStrides(weights_dims));
  for (int b = 0; b < batches; ++b) {
    for (int out_c = 0; out_c < output_depth; ++out_c) {
      float total = 0.f;
      for (int d = 0; d < accum_depth; ++d) {
        total += input_data[b * accum_depth + d] *
                 weights_data[out_c * accum_depth + d];
      }
      float bias_value = 0.0f;
      if (bias_data) {
        bias_value = bias_data[Offset(bias_dims, out_c, 0, 0, 0)];
      }
      output_data[out_c + output_depth * b] = ActivationFunctionWithMinMax(
          total + bias_value, output_activation_min, output_activation_max);
    }
  }
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void FullyConnected(const float* input_data, const Dims<4>& input_dims,
                    const float* weights_data, const Dims<4>& weights_dims,
                    const float* bias_data, const Dims<4>& bias_dims,
                    float* output_data, const Dims<4>& output_dims) {
  float output_activation_min, output_activation_max;
  GetActivationMinMax(Ac, &output_activation_min, &output_activation_max);
  FullyConnected(input_data, input_dims, weights_data, weights_dims, bias_data,
                 bias_dims, output_activation_min, output_activation_max,
                 output_data, output_dims);
}

inline void FullyConnected(const uint8* input_data, const Dims<4>& input_dims,
                           int32 input_offset, const uint8* filter_data,
                           const Dims<4>& filter_dims, int32 filter_offset,
                           const int32* bias_data, const Dims<4>& bias_dims,
                           int32 output_offset, int32 output_multiplier,
                           int output_shift, int32 output_activation_min,
                           int32 output_activation_max, uint8* output_data,
                           const Dims<4>& output_dims,
                           gemmlowp::GemmContext* gemm_context) {
  (void)gemm_context;  // only used in optimized code.
  TFLITE_DCHECK_LE(output_activation_min, output_activation_max);
  // TODO(benoitjacob): This really should be:
  //     const int batches = ArraySize(output_dims, 1);
  // but the current --variable_batch hack consists in overwriting the 3rd
  // dimension with the runtime batch size, as we don't keep track for each
  // array of which dimension is the batch dimension in it.
  const int batches = ArraySize(output_dims, 1) * ArraySize(output_dims, 2) *
                      ArraySize(output_dims, 3);
  const int output_depth = MatchingArraySize(filter_dims, 1, output_dims, 0);
  const int accum_depth = ArraySize(filter_dims, 0);
  TFLITE_DCHECK(IsPackedWithoutStrides(input_dims));
  TFLITE_DCHECK(IsPackedWithoutStrides(filter_dims));
  for (int b = 0; b < batches; ++b) {
    for (int out_c = 0; out_c < output_depth; ++out_c) {
      int32 acc = 0;
      for (int d = 0; d < accum_depth; ++d) {
        int32 input_val = input_data[b * accum_depth + d];
        int32 filter_val = filter_data[out_c * accum_depth + d];
        acc += (filter_val + filter_offset) * (input_val + input_offset);
      }
      if (bias_data) {
        acc += bias_data[Offset(bias_dims, out_c, 0, 0, 0)];
      }
      acc = MultiplyByQuantizedMultiplierSmallerThanOne(acc, output_multiplier,
                                                        output_shift);
      acc += output_offset;
      acc = std::max(acc, output_activation_min);
      acc = std::min(acc, output_activation_max);
      output_data[out_c + output_depth * b] = static_cast<uint8>(acc);
    }
  }
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void FullyConnected(const uint8* input_data, const Dims<4>& input_dims,
                    int32 input_offset, const uint8* filter_data,
                    const Dims<4>& filter_dims, int32 filter_offset,
                    const int32* bias_data, const Dims<4>& bias_dims,
                    int32 output_offset, int32 output_multiplier,
                    int output_shift, int32 output_activation_min,
                    int32 output_activation_max, uint8* output_data,
                    const Dims<4>& output_dims,
                    gemmlowp::GemmContext* gemm_context) {
  static_assert(Ac == FusedActivationFunctionType::kNone ||
                    Ac == FusedActivationFunctionType::kRelu ||
                    Ac == FusedActivationFunctionType::kRelu6 ||
                    Ac == FusedActivationFunctionType::kRelu1,
                "");
  if (Ac == FusedActivationFunctionType::kNone) {
    TFLITE_DCHECK_EQ(output_activation_min, 0);
    TFLITE_DCHECK_EQ(output_activation_max, 255);
  }
  FullyConnected(input_data, input_dims, input_offset, filter_data, filter_dims,
                 filter_offset, bias_data, bias_dims, output_offset,
                 output_multiplier, output_shift, output_activation_min,
                 output_activation_max, output_data, output_dims, gemm_context);
}

template <FusedActivationFunctionType Ac>
void NonGlobalBatchNormalization(
    const float* input_data, const Dims<4>& input_dims, const float* mean_data,
    const Dims<4>& mean_dims, const float* multiplier_data,
    const Dims<4>& multiplier_dims, const float* offset_data,
    const Dims<4>& offset_dims, float* output_data,
    const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height =
      MatchingArraySize(input_dims, 2, mean_dims, 2, multiplier_dims, 2,
                        offset_dims, 2, output_dims, 2);
  const int width =
      MatchingArraySize(input_dims, 1, mean_dims, 1, multiplier_dims, 1,
                        offset_dims, 1, output_dims, 1);
  const int depth =
      MatchingArraySize(input_dims, 0, mean_dims, 0, multiplier_dims, 0,
                        offset_dims, 0, output_dims, 0);

  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          output_data[Offset(output_dims, c, x, y, b)] = ActivationFunction<Ac>(
              (input_data[Offset(input_dims, c, x, y, b)] -
               mean_data[Offset(mean_dims, c, x, y, 0)]) *
                  multiplier_data[Offset(multiplier_dims, c, x, y, 0)] +
              offset_data[Offset(offset_dims, c, x, y, 0)]);
        }
      }
    }
  }
}

template <FusedActivationFunctionType Ac>
void GlobalBatchNormalization(const float* input_data,
                              const Dims<4>& input_dims, const float* mean_data,
                              const Dims<4>& mean_dims,
                              const float* multiplier_data,
                              const Dims<4>& multiplier_dims,
                              const float* offset_data,
                              const Dims<4>& offset_dims, float* output_data,
                              const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth =
      MatchingArraySize(input_dims, 0, mean_dims, 0, multiplier_dims, 0,
                        offset_dims, 0, output_dims, 0);

  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          output_data[Offset(output_dims, c, x, y, b)] = ActivationFunction<Ac>(
              (input_data[Offset(input_dims, c, x, y, b)] -
               mean_data[Offset(mean_dims, c, 0, 0, 0)]) *
                  multiplier_data[Offset(multiplier_dims, c, 0, 0, 0)] +
              offset_data[Offset(offset_dims, c, 0, 0, 0)]);
        }
      }
    }
  }
}

inline void Relu(const float* input_data, const Dims<4>& input_dims,
                 float* output_data, const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          float val = input_data[Offset(input_dims, c, x, y, b)];
          const float lower = 0;
          float clamped = val < lower ? lower : val;
          output_data[Offset(output_dims, c, x, y, b)] = clamped;
        }
      }
    }
  }
}

inline void Relu1(const float* input_data, const Dims<4>& input_dims,
                  float* output_data, const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          float val = input_data[Offset(input_dims, c, x, y, b)];
          const float upper = 1;
          const float lower = -1;
          float clamped = val > upper ? upper : val < lower ? lower : val;
          output_data[Offset(output_dims, c, x, y, b)] = clamped;
        }
      }
    }
  }
}

inline void Relu6(const float* input_data, const Dims<4>& input_dims,
                  float* output_data, const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          float val = input_data[Offset(input_dims, c, x, y, b)];
          const float upper = 6;
          const float lower = 0;
          float clamped = val > upper ? upper : val < lower ? lower : val;
          output_data[Offset(output_dims, c, x, y, b)] = clamped;
        }
      }
    }
  }
}

template <FusedActivationFunctionType Ac>
void L2Normalization(const float* input_data, const Dims<4>& input_dims,
                     float* output_data, const Dims<4>& output_dims) {
  static_assert(Ac == FusedActivationFunctionType::kNone, "");
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        float squared_l2_norm = 0;
        for (int c = 0; c < depth; ++c) {
          float val = input_data[Offset(input_dims, c, x, y, b)];
          squared_l2_norm += val * val;
        }
        float l2_norm = std::sqrt(squared_l2_norm);
        for (int c = 0; c < depth; ++c) {
          output_data[Offset(output_dims, c, x, y, b)] =
              input_data[Offset(input_dims, c, x, y, b)] / l2_norm;
        }
      }
    }
  }
}

inline void GetInvSqrtQuantizedMultiplier(int32 input, int32* output_inv_sqrt,
                                          int* output_shift) {
  *output_shift = 11;
  while (input >= (1 << 29)) {
    input /= 4;
    ++*output_shift;
  }
  TFLITE_DCHECK_GT(input, 0);
  const unsigned max_left_shift_bits = __builtin_clz(input) - 1;
  const unsigned max_left_shift_bit_pairs = max_left_shift_bits / 2;
  const unsigned left_shift_bit_pairs = max_left_shift_bit_pairs - 1;
  *output_shift -= left_shift_bit_pairs;
  input <<= 2 * left_shift_bit_pairs;
  TFLITE_DCHECK_GE(input, (1 << 27));
  TFLITE_DCHECK_LT(input, (1 << 29));
  using gemmlowp::FixedPoint;
  using gemmlowp::Rescale;
  using gemmlowp::SaturatingRoundingMultiplyByPOT;
  // Using 3 integer bits gives us enough room for the internal arithmetic in
  // this Newton-Raphson iteration.
  using F3 = FixedPoint<int32, 3>;
  using F0 = FixedPoint<int32, 0>;
  const F3 fixedpoint_input = F3::FromRaw(input >> 1);
  const F3 fixedpoint_half_input =
      SaturatingRoundingMultiplyByPOT<-1>(fixedpoint_input);
  const F3 fixedpoint_half_three =
      GEMMLOWP_CHECKED_FIXEDPOINT_CONSTANT(F3, (1 << 28) + (1 << 27), 1.5);
  // Newton-Raphson iteration
  // Naive unoptimized starting guess: x = 1
  F3 x = F3::One();
  // Naive unoptimized number of iterations: 5
  for (int i = 0; i < 5; i++) {
    const F3 x3 = Rescale<3>(x * x * x);
    x = Rescale<3>(fixedpoint_half_three * x - fixedpoint_half_input * x3);
  }
  const F0 fixedpoint_half_sqrt_2 =
      GEMMLOWP_CHECKED_FIXEDPOINT_CONSTANT(F0, 1518500250, std::sqrt(2.) / 2.);
  x = x * fixedpoint_half_sqrt_2;
  *output_inv_sqrt = x.raw();
  if (*output_shift < 0) {
    *output_inv_sqrt <<= -*output_shift;
    *output_shift = 0;
  }
}

inline void L2Normalization(const uint8* input_data, const Dims<4>& input_dims,
                            int32 input_zero_point, uint8* output_data,
                            const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  TFLITE_DCHECK_EQ(batches, 1);
  TFLITE_DCHECK_EQ(height, 1);
  TFLITE_DCHECK_EQ(width, 1);
  int32 square_l2_norm = 0;
  for (int i = 0; i < depth; i++) {
    int32 diff = input_data[Offset(input_dims, i, 0, 0, 0)] - input_zero_point;
    square_l2_norm += diff * diff;
  }
  int32 inv_l2norm_multiplier;
  int inv_l2norm_shift;
  GetInvSqrtQuantizedMultiplier(square_l2_norm, &inv_l2norm_multiplier,
                                &inv_l2norm_shift);

  for (int i = 0; i < depth; i++) {
    int32 diff = input_data[Offset(input_dims, i, 0, 0, 0)] - input_zero_point;
    int32 rescaled_diff = MultiplyByQuantizedMultiplierSmallerThanOne(
        128 * diff, inv_l2norm_multiplier, inv_l2norm_shift);
    int32 unclamped_output_val = 128 + rescaled_diff;
    int32 output_val = std::min(255, std::max(0, unclamped_output_val));
    output_data[Offset(output_dims, i, 0, 0, 0)] =
        static_cast<uint8>(output_val);
  }
}

inline void Add(const float* input1_data, const Dims<4>& input1_dims,
                const float* input2_data, const Dims<4>& input2_dims,
                float output_activation_min, float output_activation_max,
                float* output_data, const Dims<4>& output_dims) {
  const int batches =
      MatchingArraySize(input1_dims, 3, input2_dims, 3, output_dims, 3);
  const int height =
      MatchingArraySize(input1_dims, 2, input2_dims, 2, output_dims, 2);
  const int width =
      MatchingArraySize(input1_dims, 1, input2_dims, 1, output_dims, 1);
  const int depth =
      MatchingArraySize(input1_dims, 0, input2_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          output_data[Offset(output_dims, c, x, y, b)] =
              ActivationFunctionWithMinMax(
                  input1_data[Offset(input1_dims, c, x, y, b)] +
                      input2_data[Offset(input2_dims, c, x, y, b)],
                  output_activation_min, output_activation_max);
        }
      }
    }
  }
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void Add(const float* input1_data, const Dims<4>& input1_dims,
         const float* input2_data, const Dims<4>& input2_dims,
         float* output_data, const Dims<4>& output_dims) {
  float output_activation_min, output_activation_max;
  GetActivationMinMax(Ac, &output_activation_min, &output_activation_max);

  Add(input1_data, input1_dims, input2_data, input2_dims, output_activation_min,
      output_activation_max, output_data, output_dims);
}

template <FusedActivationFunctionType Ac>
inline void Add(int left_shift, const uint8* input1_data,
                const Dims<4>& input1_dims, int32 input1_offset,
                int32 input1_multiplier, int input1_shift,
                const uint8* input2_data, const Dims<4>& input2_dims,
                int32 input2_offset, int32 input2_multiplier, int input2_shift,
                int32 output_offset, int32 output_multiplier, int output_shift,
                int32 output_activation_min, int32 output_activation_max,
                uint8* output_data, const Dims<4>& output_dims) {
  static_assert(Ac == FusedActivationFunctionType::kNone ||
                    Ac == FusedActivationFunctionType::kRelu ||
                    Ac == FusedActivationFunctionType::kRelu6 ||
                    Ac == FusedActivationFunctionType::kRelu1,
                "");
  TFLITE_DCHECK_LE(output_activation_min, output_activation_max);
  if (Ac == FusedActivationFunctionType::kNone) {
    TFLITE_DCHECK_EQ(output_activation_min, 0);
    TFLITE_DCHECK_EQ(output_activation_max, 255);
  }
  const int batches =
      MatchingArraySize(input1_dims, 3, input2_dims, 3, output_dims, 3);
  const int height =
      MatchingArraySize(input1_dims, 2, input2_dims, 2, output_dims, 2);
  const int width =
      MatchingArraySize(input1_dims, 1, input2_dims, 1, output_dims, 1);
  const int depth =
      MatchingArraySize(input1_dims, 0, input2_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          const int32 input1_val =
              input1_offset + input1_data[Offset(input1_dims, c, x, y, b)];
          const int32 input2_val =
              input2_offset + input2_data[Offset(input2_dims, c, x, y, b)];
          const int32 shifted_input1_val = input1_val * (1 << left_shift);
          const int32 shifted_input2_val = input2_val * (1 << left_shift);
          const int32 scaled_input1_val =
              MultiplyByQuantizedMultiplierSmallerThanOne(
                  shifted_input1_val, input1_multiplier, input1_shift);
          const int32 scaled_input2_val =
              MultiplyByQuantizedMultiplierSmallerThanOne(
                  shifted_input2_val, input2_multiplier, input2_shift);
          const int32 raw_sum = scaled_input1_val + scaled_input2_val;
          const int32 raw_output =
              MultiplyByQuantizedMultiplierSmallerThanOne(
                  raw_sum, output_multiplier, output_shift) +
              output_offset;
          const int32 clamped_output =
              std::min(output_activation_max,
                       std::max(output_activation_min, raw_output));
          output_data[Offset(output_dims, c, x, y, b)] =
              static_cast<uint8>(clamped_output);
        }
      }
    }
  }
}

// TODO(jiawen): We can implement BroadcastAdd on buffers of arbitrary
// dimensionality if the runtime code does a single loop over one dimension
// that handles broadcasting as the base case. The code generator would then
// generate max(D1, D2) nested for loops.
template <FusedActivationFunctionType Ac>
void BroadcastAdd(const float* input1_data, const Dims<4>& input1_dims,
                  const float* input2_data, const Dims<4>& input2_dims,
                  float* output_data, const Dims<4>& output_dims) {
  gemmlowp::ScopedProfilingLabel label("BroadcastAdd");

  NdArrayDesc<4> desc1;
  NdArrayDesc<4> desc2;
  NdArrayDescsForElementwiseBroadcast(input1_dims, input2_dims, &desc1, &desc2);

  // In Tensorflow, the dimensions are canonically named (batch_number, row,
  // col, channel), with extents (batches, height, width, depth), with the
  // trailing dimension changing most rapidly (channels has the smallest stride,
  // typically 1 element).
  //
  // In generated C code, we store arrays with the dimensions reversed. The
  // first dimension has smallest stride.
  //
  // We name our variables by their Tensorflow convention, but generate C code
  // nesting loops such that the innermost loop has the smallest stride for the
  // best cache behavior.
  for (int b = 0; b < ArraySize(output_dims, 3); ++b) {
    for (int y = 0; y < ArraySize(output_dims, 2); ++y) {
      for (int x = 0; x < ArraySize(output_dims, 1); ++x) {
        for (int c = 0; c < ArraySize(output_dims, 0); ++c) {
          output_data[Offset(output_dims, c, x, y, b)] = ActivationFunction<Ac>(
              input1_data[SubscriptToIndex(desc1, c, x, y, b)] +
              input2_data[SubscriptToIndex(desc2, c, x, y, b)]);
        }
      }
    }
  }
}

inline void BroadcastAdd(int left_shift, const uint8* input1_data,
                         const Dims<4>& input1_dims, int32 input1_offset,
                         int32 input1_multiplier, int input1_shift,
                         const uint8* input2_data, const Dims<4>& input2_dims,
                         int32 input2_offset, int32 input2_multiplier,
                         int input2_shift, int32 output_offset,
                         int32 output_multiplier, int output_shift,
                         int32 output_activation_min,
                         int32 output_activation_max, uint8* output_data,
                         const Dims<4>& output_dims) {
  gemmlowp::ScopedProfilingLabel label("BroadcastAdd/8bit");

  NdArrayDesc<4> desc1;
  NdArrayDesc<4> desc2;
  NdArrayDescsForElementwiseBroadcast(input1_dims, input2_dims, &desc1, &desc2);

  // In Tensorflow, the dimensions are canonically named (batch_number, row,
  // col, channel), with extents (batches, height, width, depth), with the
  // trailing dimension changing most rapidly (channels has the smallest stride,
  // typically 1 element).
  //
  // In generated C code, we store arrays with the dimensions reversed. The
  // first dimension has smallest stride.
  //
  // We name our variables by their Tensorflow convention, but generate C code
  // nesting loops such that the innermost loop has the smallest stride for the
  // best cache behavior.
  for (int b = 0; b < ArraySize(output_dims, 3); ++b) {
    for (int y = 0; y < ArraySize(output_dims, 2); ++y) {
      for (int x = 0; x < ArraySize(output_dims, 1); ++x) {
        for (int c = 0; c < ArraySize(output_dims, 0); ++c) {
          const int32 input1_val =
              input1_offset + input1_data[SubscriptToIndex(desc1, c, x, y, b)];
          const int32 input2_val =
              input2_offset + input2_data[SubscriptToIndex(desc2, c, x, y, b)];
          const int32 shifted_input1_val = input1_val * (1 << left_shift);
          const int32 shifted_input2_val = input2_val * (1 << left_shift);
          const int32 scaled_input1_val =
              MultiplyByQuantizedMultiplierSmallerThanOne(
                  shifted_input1_val, input1_multiplier, input1_shift);
          const int32 scaled_input2_val =
              MultiplyByQuantizedMultiplierSmallerThanOne(
                  shifted_input2_val, input2_multiplier, input2_shift);
          const int32 raw_sum = scaled_input1_val + scaled_input2_val;
          const int32 raw_output =
              MultiplyByQuantizedMultiplierSmallerThanOne(
                  raw_sum, output_multiplier, output_shift) +
              output_offset;
          const int32 clamped_output =
              std::min(output_activation_max,
                       std::max(output_activation_min, raw_output));
          output_data[Offset(output_dims, c, x, y, b)] =
              static_cast<uint8>(clamped_output);
        }
      }
    }
  }
}

template <FusedActivationFunctionType Ac>
inline void BroadcastAdd(int left_shift, const uint8* input1_data,
                         const Dims<4>& input1_dims, int32 input1_offset,
                         int32 input1_multiplier, int input1_shift,
                         const uint8* input2_data, const Dims<4>& input2_dims,
                         int32 input2_offset, int32 input2_multiplier,
                         int input2_shift, int32 output_offset,
                         int32 output_multiplier, int output_shift,
                         int32 output_activation_min,
                         int32 output_activation_max, uint8* output_data,
                         const Dims<4>& output_dims) {
  static_assert(Ac == FusedActivationFunctionType::kNone ||
                    Ac == FusedActivationFunctionType::kRelu ||
                    Ac == FusedActivationFunctionType::kRelu6 ||
                    Ac == FusedActivationFunctionType::kRelu1,
                "");
  TFLITE_DCHECK_LE(output_activation_min, output_activation_max);
  if (Ac == FusedActivationFunctionType::kNone) {
    TFLITE_DCHECK_EQ(output_activation_min, 0);
    TFLITE_DCHECK_EQ(output_activation_max, 255);
  }
  BroadcastAdd(left_shift, input1_data, input1_dims, input1_offset,
               input1_multiplier, input1_shift, input2_data, input2_dims,
               input2_offset, input2_multiplier, input2_shift, output_offset,
               output_multiplier, output_shift, output_activation_min,
               output_activation_max, output_data, output_dims);
}

inline void Mul(const float* input1_data, const Dims<4>& input1_dims,
                const float* input2_data, const Dims<4>& input2_dims,
                float output_activation_min, float output_activation_max,
                float* output_data, const Dims<4>& output_dims) {
  const int batches =
      MatchingArraySize(input1_dims, 3, input2_dims, 3, output_dims, 3);
  const int height =
      MatchingArraySize(input1_dims, 2, input2_dims, 2, output_dims, 2);
  const int width =
      MatchingArraySize(input1_dims, 1, input2_dims, 1, output_dims, 1);
  const int depth =
      MatchingArraySize(input1_dims, 0, input2_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          output_data[Offset(output_dims, c, x, y, b)] =
              ActivationFunctionWithMinMax(
                  input1_data[Offset(input1_dims, c, x, y, b)] *
                      input2_data[Offset(input2_dims, c, x, y, b)],
                  output_activation_min, output_activation_max);
        }
      }
    }
  }
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void Mul(const float* input1_data, const Dims<4>& input1_dims,
         const float* input2_data, const Dims<4>& input2_dims,
         float* output_data, const Dims<4>& output_dims) {
  float output_activation_min, output_activation_max;
  GetActivationMinMax(Ac, &output_activation_min, &output_activation_max);

  Mul(input1_data, input1_dims, input2_data, input2_dims, output_activation_min,
      output_activation_max, output_data, output_dims);
}

// TODO(jiawen): We can implement BroadcastMul on buffers of arbitrary
// dimensionality if the runtime code does a single loop over one dimension
// that handles broadcasting as the base case. The code generator would then
// generate max(D1, D2) nested for loops.
template <FusedActivationFunctionType Ac>
void BroadcastMul(const float* input1_data, const Dims<4>& input1_dims,
                  const float* input2_data, const Dims<4>& input2_dims,
                  float* output_data, const Dims<4>& output_dims) {
  gemmlowp::ScopedProfilingLabel label("BroadcastMul");

  NdArrayDesc<4> desc1;
  NdArrayDesc<4> desc2;
  NdArrayDescsForElementwiseBroadcast(input1_dims, input2_dims, &desc1, &desc2);

  // In Tensorflow, the dimensions are canonically named (batch_number, row,
  // col, channel), with extents (batches, height, width, depth), with the
  // trailing dimension changing most rapidly (channels has the smallest
  // stride, typically 1 element).
  //
  // In generated C code, we store arrays with the dimensions reversed. The
  // first dimension has smallest stride.
  //
  // We name our variables by their Tensorflow convention, but generate C code
  // nesting loops such that the innermost loop has the smallest stride for
  // the best cache behavior.
  for (int b = 0; b < ArraySize(output_dims, 3); ++b) {
    for (int y = 0; y < ArraySize(output_dims, 2); ++y) {
      for (int x = 0; x < ArraySize(output_dims, 1); ++x) {
        for (int c = 0; c < ArraySize(output_dims, 0); ++c) {
          output_data[Offset(output_dims, c, x, y, b)] = ActivationFunction<Ac>(
              input1_data[SubscriptToIndex(desc1, c, x, y, b)] *
              input2_data[SubscriptToIndex(desc2, c, x, y, b)]);
        }
      }
    }
  }
}

inline void BroadcastMul(const uint8* input1_data, const Dims<4>& input1_dims,
                         int32 input1_offset, const uint8* input2_data,
                         const Dims<4>& input2_dims, int32 input2_offset,
                         int32 output_offset, int32 output_multiplier,
                         int output_shift, int32 output_activation_min,
                         int32 output_activation_max, uint8* output_data,
                         const Dims<4>& output_dims) {
  gemmlowp::ScopedProfilingLabel label("BroadcastMul/8bit");

  NdArrayDesc<4> desc1;
  NdArrayDesc<4> desc2;
  NdArrayDescsForElementwiseBroadcast(input1_dims, input2_dims, &desc1, &desc2);

  // In Tensorflow, the dimensions are canonically named (batch_number, row,
  // col, channel), with extents (batches, height, width, depth), with the
  // trailing dimension changing most rapidly (channels has the smallest
  // stride, typically 1 element).
  //
  // In generated C code, we store arrays with the dimensions reversed. The
  // first dimension has smallest stride.
  //
  // We name our variables by their Tensorflow convention, but generate C code
  // nesting loops such that the innermost loop has the smallest stride for
  // the best cache behavior.
  for (int b = 0; b < ArraySize(output_dims, 3); ++b) {
    for (int y = 0; y < ArraySize(output_dims, 2); ++y) {
      for (int x = 0; x < ArraySize(output_dims, 1); ++x) {
        for (int c = 0; c < ArraySize(output_dims, 0); ++c) {
          const int32 input1_val =
              input1_offset + input1_data[SubscriptToIndex(desc1, c, x, y, b)];
          const int32 input2_val =
              input2_offset + input2_data[SubscriptToIndex(desc2, c, x, y, b)];
          const int32 unclamped_result =
              output_offset +
              MultiplyByQuantizedMultiplierSmallerThanOne(
                  input1_val * input2_val, output_multiplier, output_shift);
          const int32 clamped_output =
              std::min(output_activation_max,
                       std::max(output_activation_min, unclamped_result));
          output_data[Offset(output_dims, c, x, y, b)] =
              static_cast<uint8>(clamped_output);
        }
      }
    }
  }
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
inline void BroadcastMul(const uint8* input1_data, const Dims<4>& input1_dims,
                         int32 input1_offset, const uint8* input2_data,
                         const Dims<4>& input2_dims, int32 input2_offset,
                         int32 output_offset, int32 output_multiplier,
                         int output_shift, int32 output_activation_min,
                         int32 output_activation_max, uint8* output_data,
                         const Dims<4>& output_dims) {
  BroadcastMul(input1_data, input1_dims, input1_offset, input2_data,
               input2_dims, input2_offset, output_offset, output_multiplier,
               output_shift, output_activation_min, output_activation_max,
               output_data, output_dims);
}

template <FusedActivationFunctionType Ac, typename Scalar>
void Concatenation(int concat_dim, const Scalar* const* input_data,
                   const Dims<4>* const* input_dims, int inputs_count,
                   Scalar* output_data, const Dims<4>& output_dims) {
  TFLITE_DCHECK_GT(inputs_count, 1);
  int concat_size = 0;
  for (int i = 0; i < inputs_count; i++) {
    for (int j = 0; j < 4; j++) {
      if (j != concat_dim) {
        MatchingArraySize(*input_dims[i], j, output_dims, j);
      }
    }
    concat_size += ArraySize(*input_dims[i], concat_dim);
  }
  TFLITE_DCHECK_EQ(concat_size, ArraySize(output_dims, concat_dim));
  TFLITE_DCHECK(Ac == FusedActivationFunctionType::kNone);
  int outer_size = 1;
  for (int i = concat_dim + 1; i < 4; i++) {
    outer_size *= output_dims.sizes[i];
  }
  Scalar* output_ptr = output_data;
  for (int k = 0; k < outer_size; k++) {
    for (int i = 0; i < inputs_count; ++i) {
      const int copy_size =
          input_dims[i]->sizes[concat_dim] * input_dims[i]->strides[concat_dim];
      memcpy(output_ptr, input_data[i] + k * copy_size,
             copy_size * sizeof(Scalar));
      output_ptr += copy_size;
    }
  }
}

template <FusedActivationFunctionType Ac, typename Scalar>
void DepthConcatenation(const Scalar* const* input_data,
                        const Dims<4>* const* input_dims, int inputs_count,
                        Scalar* output_data, const Dims<4>& output_dims) {
  Concatenation<Ac, Scalar>(0, input_data, input_dims, inputs_count,
                            output_data, output_dims);
}

inline void LstmCell(const float* input_data, const Dims<4>& input_dims,
                     const float* prev_activ_data,
                     const Dims<4>& prev_activ_dims, const float* weights_data,
                     const Dims<4>& weights_dims, const float* bias_data,
                     const Dims<4>& bias_dims, const float* prev_state_data,
                     const Dims<4>& prev_state_dims, float* output_state_data,
                     const Dims<4>& output_state_dims, float* output_activ_data,
                     const Dims<4>& output_activ_dims, float* concat_temp_data,
                     const Dims<4>& concat_temp_dims, float* activ_temp_data,
                     const Dims<4>& activ_temp_dims) {
  const int batches =
      MatchingArraySize(input_dims, 3, prev_activ_dims, 3, prev_state_dims, 3,
                        output_state_dims, 3, output_activ_dims, 3);
  const int height =
      MatchingArraySize(input_dims, 2, prev_activ_dims, 2, prev_state_dims, 2,
                        output_state_dims, 2, output_activ_dims, 2);
  const int width =
      MatchingArraySize(input_dims, 1, prev_activ_dims, 1, prev_state_dims, 1,
                        output_state_dims, 1, output_activ_dims, 1);
  TFLITE_CHECK_EQ(ArraySize(weights_dims, 2), 1);
  TFLITE_CHECK_EQ(ArraySize(weights_dims, 3), 1);
  const int input_depth = ArraySize(input_dims, 0);
  const int prev_activ_depth = ArraySize(prev_activ_dims, 0);
  const int total_input_depth = prev_activ_depth + input_depth;
  TFLITE_CHECK_EQ(ArraySize(weights_dims, 0), total_input_depth);
  TFLITE_CHECK_EQ(MatchingArraySize(bias_dims, 1, bias_dims, 2, bias_dims, 3),
                  1);
  const int intern_activ_depth =
      MatchingArraySize(weights_dims, 1, bias_dims, 0);
  TFLITE_CHECK_EQ(intern_activ_depth % 4, 0);
  const int output_depth =
      MatchingArraySize(prev_state_dims, 0, prev_activ_dims, 0,
                        output_state_dims, 0, output_activ_dims, 0);
  TFLITE_CHECK_EQ(output_depth, intern_activ_depth / 4);

  // Concatenate prev_activ and input data together
  std::vector<float const*> concat_input_arrays_data;
  std::vector<Dims<4> const*> concat_input_arrays_dims;
  concat_input_arrays_data.push_back(input_data);
  concat_input_arrays_data.push_back(prev_activ_data);
  concat_input_arrays_dims.push_back(&input_dims);
  concat_input_arrays_dims.push_back(&prev_activ_dims);
  Concatenation<FusedActivationFunctionType::kNone, float>(
      0, &(concat_input_arrays_data[0]), &(concat_input_arrays_dims[0]),
      concat_input_arrays_data.size(), concat_temp_data, concat_temp_dims);

  // Fully connected
  FullyConnected<FusedActivationFunctionType::kNone>(
      concat_temp_data, concat_temp_dims, weights_data, weights_dims, bias_data,
      bias_dims, activ_temp_data, activ_temp_dims);

  // Memory state update (the LSTM "guts")
  for (int b = 0; b < batches; ++b) {
    for (int w = 0; w < width; ++w) {
      for (int h = 0; h < height; ++h) {
        for (int c = 0; c < output_depth; ++c) {
          const float input_gate =
              1.f /
              (1.f + std::exp(-activ_temp_data[Offset(
                         activ_temp_dims, 0 * output_depth + c, w, h, b)]));
          const float new_input = std::tanh(activ_temp_data[Offset(
              activ_temp_dims, 1 * output_depth + c, w, h, b)]);
          const float forget_gate =
              1.f /
              (1.f + std::exp(-activ_temp_data[Offset(
                         activ_temp_dims, 2 * output_depth + c, w, h, b)]));
          const float output_gate =
              1.f /
              (1.f + std::exp(-activ_temp_data[Offset(
                         activ_temp_dims, 3 * output_depth + c, w, h, b)]));
          const float new_state =
              input_gate * new_input +
              forget_gate *
                  prev_state_data[Offset(prev_state_dims, c, w, h, b)];
          output_state_data[Offset(output_state_dims, c, w, h, b)] = new_state;
          output_activ_data[Offset(output_activ_dims, c, w, h, b)] =
              output_gate * std::tanh(new_state);
        }
      }
    }
  }
}

template <FusedActivationFunctionType Ac, typename Scalar>
void TensorFlowSplit(const Scalar* input_data, const Dims<4>& input_dims,
                     int outputs_count, Scalar* const* output_data,
                     const Dims<4>* const* output_dims) {
  TFLITE_DCHECK_GE(outputs_count, 1);
  for (int i = 0; i < outputs_count; i++) {
    /* batches = */ MatchingArraySize(*output_dims[i], 3, input_dims, 3);
    /* height = */ MatchingArraySize(*output_dims[i], 2, input_dims, 2);
    /* width = */ MatchingArraySize(*output_dims[i], 1, input_dims, 1);
  }
  const int batches = MatchingArraySize(*output_dims[0], 3, input_dims, 3);
  const int height = MatchingArraySize(*output_dims[0], 2, input_dims, 2);
  const int width = MatchingArraySize(*output_dims[0], 1, input_dims, 1);
  // for now we dont have a model with a TensorFlowSplit
  // with fused activation function.
  TFLITE_DCHECK(Ac == FusedActivationFunctionType::kNone);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        int in_c = 0;
        for (int i = 0; i < outputs_count; ++i) {
          const int depth = ArraySize(*output_dims[i], 0);
          for (int c = 0; c < depth; ++c) {
            output_data[i][Offset(*output_dims[i], c, x, y, b)] =
                input_data[Offset(input_dims, in_c, x, y, b)];
            in_c++;
          }
        }
        TFLITE_DCHECK(in_c == ArraySize(input_dims, 0));
      }
    }
  }
}

// TODO(benoitjacob) make this a proper reference impl without Eigen!
template <typename Scalar>
using MatrixMap = typename std::conditional<
    std::is_const<Scalar>::value,
    Eigen::Map<const Eigen::Matrix<typename std::remove_const<Scalar>::type,
                                   Eigen::Dynamic, Eigen::Dynamic>>,
    Eigen::Map<Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>>>::type;

template <typename Scalar, int N>
MatrixMap<Scalar> MapAsMatrixWithFirstDimAsRows(Scalar* data,
                                                const Dims<N>& dims) {
  const int rows = dims.sizes[0];
  int cols = 1;
  for (int d = 1; d < N; d++) {
    cols *= dims.sizes[d];
  }
  return MatrixMap<Scalar>(data, rows, cols);
}

template <typename Scalar, int N>
MatrixMap<Scalar> MapAsMatrixWithLastDimAsCols(Scalar* data,
                                               const Dims<N>& dims) {
  const int cols = dims.sizes[N - 1];
  int rows = 1;
  for (int d = 0; d < N - 1; d++) {
    rows *= dims.sizes[d];
  }
  return MatrixMap<Scalar>(data, rows, cols);
}

inline int NodeOffset(int b, int h, int w, int height, int width) {
  return (b * height + h) * width + w;
}

inline void AveragePool(const float* input_data, const Dims<4>& input_dims,
                        int stride_width, int stride_height, int pad_width,
                        int pad_height, int filter_width, int filter_height,
                        float output_activation_min,
                        float output_activation_max, float* output_data,
                        const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  const int input_height = ArraySize(input_dims, 2);
  const int input_width = ArraySize(input_dims, 1);
  const int output_height = ArraySize(output_dims, 2);
  const int output_width = ArraySize(output_dims, 1);
  for (int batch = 0; batch < batches; ++batch) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      for (int out_x = 0; out_x < output_width; ++out_x) {
        for (int channel = 0; channel < depth; ++channel) {
          const int in_x_origin = (out_x * stride_width) - pad_width;
          const int in_y_origin = (out_y * stride_height) - pad_height;
          // Compute the boundaries of the filter region clamped so as to
          // ensure that the filter window fits in the input array.
          const int filter_x_start = std::max(0, -in_x_origin);
          const int filter_x_end =
              std::min(filter_width, input_width - in_x_origin);
          const int filter_y_start = std::max(0, -in_y_origin);
          const int filter_y_end =
              std::min(filter_height, input_height - in_y_origin);
          float total = 0.f;
          float filter_count = 0;
          for (int filter_y = filter_y_start; filter_y < filter_y_end;
               ++filter_y) {
            for (int filter_x = filter_x_start; filter_x < filter_x_end;
                 ++filter_x) {
              const int in_x = in_x_origin + filter_x;
              const int in_y = in_y_origin + filter_y;
              total +=
                  input_data[Offset(input_dims, channel, in_x, in_y, batch)];
              filter_count++;
            }
          }
          const float average = total / filter_count;
          output_data[Offset(output_dims, channel, out_x, out_y, batch)] =
              ActivationFunctionWithMinMax(average, output_activation_min,
                                           output_activation_max);
        }
      }
    }
  }
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void AveragePool(const float* input_data, const Dims<4>& input_dims,
                 int stride_width, int stride_height, int pad_width,
                 int pad_height, int filter_width, int filter_height,
                 float* output_data, const Dims<4>& output_dims) {
  float output_activation_min, output_activation_max;
  GetActivationMinMax(Ac, &output_activation_min, &output_activation_max);
  AveragePool(input_data, input_dims, stride_width, stride_height, pad_width,
              pad_height, filter_width, filter_height, output_activation_min,
              output_activation_max, output_data, output_dims);
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void AveragePool(const float* input_data, const Dims<4>& input_dims, int stride,
                 int pad_width, int pad_height, int filter_width,
                 int filter_height, float* output_data,
                 const Dims<4>& output_dims) {
  AveragePool<Ac>(input_data, input_dims, stride, stride, pad_width, pad_height,
                  filter_width, filter_height, output_data, output_dims);
}

inline void AveragePool(const uint8* input_data, const Dims<4>& input_dims,
                        int stride_width, int stride_height, int pad_width,
                        int pad_height, int filter_width, int filter_height,
                        int32 output_activation_min,
                        int32 output_activation_max, uint8* output_data,
                        const Dims<4>& output_dims) {
  TFLITE_DCHECK_LE(output_activation_min, output_activation_max);
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  const int input_height = ArraySize(input_dims, 2);
  const int input_width = ArraySize(input_dims, 1);
  const int output_height = ArraySize(output_dims, 2);
  const int output_width = ArraySize(output_dims, 1);
  for (int batch = 0; batch < batches; ++batch) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      for (int out_x = 0; out_x < output_width; ++out_x) {
        for (int channel = 0; channel < depth; ++channel) {
          const int in_x_origin = (out_x * stride_width) - pad_width;
          const int in_y_origin = (out_y * stride_height) - pad_height;
          // Compute the boundaries of the filter region clamped so as to
          // ensure that the filter window fits in the input array.
          const int filter_x_start = std::max(0, -in_x_origin);
          const int filter_x_end =
              std::min(filter_width, input_width - in_x_origin);
          const int filter_y_start = std::max(0, -in_y_origin);
          const int filter_y_end =
              std::min(filter_height, input_height - in_y_origin);
          int32 acc = 0;
          int filter_count = 0;
          for (int filter_y = filter_y_start; filter_y < filter_y_end;
               ++filter_y) {
            for (int filter_x = filter_x_start; filter_x < filter_x_end;
                 ++filter_x) {
              const int in_x = in_x_origin + filter_x;
              const int in_y = in_y_origin + filter_y;
              acc += input_data[Offset(input_dims, channel, in_x, in_y, batch)];
              filter_count++;
            }
          }
          acc = (acc + filter_count / 2) / filter_count;
          acc = std::max(acc, output_activation_min);
          acc = std::min(acc, output_activation_max);
          output_data[Offset(output_dims, channel, out_x, out_y, batch)] =
              static_cast<uint8>(acc);
        }
      }
    }
  }
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void AveragePool(const uint8* input_data, const Dims<4>& input_dims,
                 int stride_width, int stride_height, int pad_width,
                 int pad_height, int filter_width, int filter_height,
                 int32 output_activation_min, int32 output_activation_max,
                 uint8* output_data, const Dims<4>& output_dims) {
  static_assert(Ac == FusedActivationFunctionType::kNone ||
                    Ac == FusedActivationFunctionType::kRelu ||
                    Ac == FusedActivationFunctionType::kRelu6 ||
                    Ac == FusedActivationFunctionType::kRelu1,
                "");
  if (Ac == FusedActivationFunctionType::kNone) {
    TFLITE_DCHECK_EQ(output_activation_min, 0);
    TFLITE_DCHECK_EQ(output_activation_max, 255);
  }
  AveragePool(input_data, input_dims, stride_width, stride_height, pad_width,
              pad_height, filter_width, filter_height, output_activation_min,
              output_activation_max, output_data, output_dims);
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void AveragePool(const uint8* input_data, const Dims<4>& input_dims, int stride,
                 int pad_width, int pad_height, int filter_width,
                 int filter_height, int32 output_activation_min,
                 int32 output_activation_max, uint8* output_data,
                 const Dims<4>& output_dims) {
  AveragePool<Ac>(input_data, input_dims, stride, stride, pad_width, pad_height,
                  filter_width, filter_height, output_activation_min,
                  output_activation_max, output_data, output_dims);
}

inline void L2Pool(const float* input_data, const Dims<4>& input_dims,
                   int stride_width, int stride_height, int pad_width,
                   int pad_height, int filter_width, int filter_height,
                   float output_activation_min, float output_activation_max,
                   float* output_data, const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  const int input_height = ArraySize(input_dims, 2);
  const int input_width = ArraySize(input_dims, 1);
  const int output_height = ArraySize(output_dims, 2);
  const int output_width = ArraySize(output_dims, 1);
  for (int batch = 0; batch < batches; ++batch) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      for (int out_x = 0; out_x < output_width; ++out_x) {
        for (int channel = 0; channel < depth; ++channel) {
          const int in_x_origin = (out_x * stride_width) - pad_width;
          const int in_y_origin = (out_y * stride_height) - pad_height;
          // Compute the boundaries of the filter region clamped so as to
          // ensure that the filter window fits in the input array.
          const int filter_x_start = std::max(0, -in_x_origin);
          const int filter_x_end =
              std::min(filter_width, input_width - in_x_origin);
          const int filter_y_start = std::max(0, -in_y_origin);
          const int filter_y_end =
              std::min(filter_height, input_height - in_y_origin);
          float sum_squares = 0.f;
          int filter_count = 0;
          for (int filter_y = filter_y_start; filter_y < filter_y_end;
               ++filter_y) {
            for (int filter_x = filter_x_start; filter_x < filter_x_end;
                 ++filter_x) {
              const int in_x = in_x_origin + filter_x;
              const int in_y = in_y_origin + filter_y;
              const float val =
                  input_data[Offset(input_dims, channel, in_x, in_y, batch)];
              sum_squares += val * val;
              filter_count++;
            }
          }
          const float l2pool_result = std::sqrt(sum_squares / filter_count);
          output_data[Offset(output_dims, channel, out_x, out_y, batch)] =
              ActivationFunctionWithMinMax(l2pool_result, output_activation_min,
                                           output_activation_max);
        }
      }
    }
  }
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void L2Pool(const float* input_data, const Dims<4>& input_dims,
            int stride_width, int stride_height, int pad_width, int pad_height,
            int filter_width, int filter_height, float* output_data,
            const Dims<4>& output_dims) {
  float output_activation_min, output_activation_max;
  GetActivationMinMax(Ac, &output_activation_min, &output_activation_max);

  L2Pool(input_data, input_dims, stride_width, stride_height, pad_width,
         pad_height, filter_width, filter_height, output_activation_min,
         output_activation_max, output_data, output_dims);
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void L2Pool(const float* input_data, const Dims<4>& input_dims, int stride,
            int pad_width, int pad_height, int filter_width, int filter_height,
            float* output_data, const Dims<4>& output_dims) {
  L2Pool<Ac>(input_data, input_dims, stride, stride, pad_width, pad_height,
             filter_width, filter_height, output_data, output_dims);
}

inline void MaxPool(const float* input_data, const Dims<4>& input_dims,
                    int stride_width, int stride_height, int pad_width,
                    int pad_height, int filter_width, int filter_height,
                    float output_activation_min, float output_activation_max,
                    float* output_data, const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  const int input_height = ArraySize(input_dims, 2);
  const int input_width = ArraySize(input_dims, 1);
  const int output_height = ArraySize(output_dims, 2);
  const int output_width = ArraySize(output_dims, 1);
  for (int batch = 0; batch < batches; ++batch) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      for (int out_x = 0; out_x < output_width; ++out_x) {
        for (int channel = 0; channel < depth; ++channel) {
          const int in_x_origin = (out_x * stride_width) - pad_width;
          const int in_y_origin = (out_y * stride_height) - pad_height;
          // Compute the boundaries of the filter region clamped so as to
          // ensure that the filter window fits in the input array.
          const int filter_x_start = std::max(0, -in_x_origin);
          const int filter_x_end =
              std::min(filter_width, input_width - in_x_origin);
          const int filter_y_start = std::max(0, -in_y_origin);
          const int filter_y_end =
              std::min(filter_height, input_height - in_y_origin);
          float max = std::numeric_limits<float>::lowest();
          for (int filter_y = filter_y_start; filter_y < filter_y_end;
               ++filter_y) {
            for (int filter_x = filter_x_start; filter_x < filter_x_end;
                 ++filter_x) {
              const int in_x = in_x_origin + filter_x;
              const int in_y = in_y_origin + filter_y;
              max = std::max(
                  max,
                  input_data[Offset(input_dims, channel, in_x, in_y, batch)]);
            }
          }
          output_data[Offset(output_dims, channel, out_x, out_y, batch)] =
              ActivationFunctionWithMinMax(max, output_activation_min,
                                           output_activation_max);
        }
      }
    }
  }
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void MaxPool(const float* input_data, const Dims<4>& input_dims,
             int stride_width, int stride_height, int pad_width, int pad_height,
             int filter_width, int filter_height, float* output_data,
             const Dims<4>& output_dims) {
  float output_activation_min, output_activation_max;
  GetActivationMinMax(Ac, &output_activation_min, &output_activation_max);
  MaxPool(input_data, input_dims, stride_width, stride_height, pad_width,
          pad_height, filter_width, filter_height, output_activation_min,
          output_activation_max, output_data, output_dims);
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void MaxPool(const float* input_data, const Dims<4>& input_dims, int stride,
             int pad_width, int pad_height, int filter_width, int filter_height,
             float* output_data, const Dims<4>& output_dims) {
  MaxPool<Ac>(input_data, input_dims, stride, stride, pad_width, pad_height,
              filter_width, filter_height, output_data, output_dims);
}

inline void MaxPool(const uint8* input_data, const Dims<4>& input_dims,
                    int stride_width, int stride_height, int pad_width,
                    int pad_height, int filter_width, int filter_height,
                    int32 output_activation_min, int32 output_activation_max,
                    uint8* output_data, const Dims<4>& output_dims) {
  TFLITE_DCHECK_LE(output_activation_min, output_activation_max);
  TFLITE_DCHECK_GE(output_activation_min, 0);
  TFLITE_DCHECK_LE(output_activation_max, 255);
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  const int input_height = ArraySize(input_dims, 2);
  const int input_width = ArraySize(input_dims, 1);
  const int output_height = ArraySize(output_dims, 2);
  const int output_width = ArraySize(output_dims, 1);
  for (int batch = 0; batch < batches; ++batch) {
    for (int out_y = 0; out_y < output_height; ++out_y) {
      for (int out_x = 0; out_x < output_width; ++out_x) {
        for (int channel = 0; channel < depth; ++channel) {
          const int in_x_origin = (out_x * stride_width) - pad_width;
          const int in_y_origin = (out_y * stride_height) - pad_height;
          // Compute the boundaries of the filter region clamped so as to
          // ensure that the filter window fits in the input array.
          const int filter_x_start = std::max(0, -in_x_origin);
          const int filter_x_end =
              std::min(filter_width, input_width - in_x_origin);
          const int filter_y_start = std::max(0, -in_y_origin);
          const int filter_y_end =
              std::min(filter_height, input_height - in_y_origin);
          uint8 max = 0;
          for (int filter_y = filter_y_start; filter_y < filter_y_end;
               ++filter_y) {
            for (int filter_x = filter_x_start; filter_x < filter_x_end;
                 ++filter_x) {
              const int in_x = in_x_origin + filter_x;
              const int in_y = in_y_origin + filter_y;
              max = std::max(
                  max,
                  input_data[Offset(input_dims, channel, in_x, in_y, batch)]);
            }
          }
          max = std::max<uint8>(max, output_activation_min);
          max = std::min<uint8>(max, output_activation_max);
          output_data[Offset(output_dims, channel, out_x, out_y, batch)] =
              static_cast<uint8>(max);
        }
      }
    }
  }
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void MaxPool(const uint8* input_data, const Dims<4>& input_dims,
             int stride_width, int stride_height, int pad_width, int pad_height,
             int filter_width, int filter_height, int32 output_activation_min,
             int32 output_activation_max, uint8* output_data,
             const Dims<4>& output_dims) {
  static_assert(Ac == FusedActivationFunctionType::kNone ||
                    Ac == FusedActivationFunctionType::kRelu ||
                    Ac == FusedActivationFunctionType::kRelu6 ||
                    Ac == FusedActivationFunctionType::kRelu1,
                "");
  if (Ac == FusedActivationFunctionType::kNone) {
    TFLITE_DCHECK_EQ(output_activation_min, 0);
    TFLITE_DCHECK_EQ(output_activation_max, 255);
  }
  MaxPool(input_data, input_dims, stride_width, stride_height, pad_width,
          pad_height, filter_width, filter_height, output_activation_min,
          output_activation_max, output_data, output_dims);
}

// legacy, for compatibility with old checked-in code
template <FusedActivationFunctionType Ac>
void MaxPool(const uint8* input_data, const Dims<4>& input_dims, int stride,
             int pad_width, int pad_height, int filter_width, int filter_height,
             int32 output_activation_min, int32 output_activation_max,
             uint8* output_data, const Dims<4>& output_dims) {
  MaxPool<Ac>(input_data, input_dims, stride, stride, pad_width, pad_height,
              filter_width, filter_height, output_activation_min,
              output_activation_max, output_data, output_dims);
}

inline void LocalResponseNormalization(const float* input_data,
                                       const Dims<4>& input_dims, int range,
                                       float bias, float alpha, float beta,
                                       float* output_data,
                                       const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);

  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          const int begin_input_c = std::max(0, c - range);
          const int end_input_c = std::min(depth, c + range);
          float accum = 0.f;
          for (int input_c = begin_input_c; input_c < end_input_c; ++input_c) {
            const float input_val =
                input_data[Offset(input_dims, input_c, x, y, b)];
            accum += input_val * input_val;
          }
          const float multiplier = std::pow(bias + alpha * accum, -beta);
          output_data[Offset(output_dims, c, x, y, b)] =
              input_data[Offset(input_dims, c, x, y, b)] * multiplier;
        }
      }
    }
  }
}

inline void Softmax(const float* input_data, const Dims<4>& input_dims,
                    float beta, float* output_data,
                    const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);

  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        // Find max element value which we'll use to ensure numerical stability
        // taking advantage of the following equality:
        // exp(x[i])/sum(exp(x[i])) == exp(x[i]+C)/sum(exp(x[i]+C))
        float max = std::numeric_limits<float>::lowest();
        for (int c = 0; c < depth; ++c) {
          max = std::max(max, input_data[Offset(input_dims, c, x, y, b)]);
        }

        // Compute sum.
        float sum = 0.f;
        for (int c = 0; c < depth; ++c) {
          sum += std::exp((input_data[Offset(input_dims, c, x, y, b)] - max) *
                          beta);
        }

        // Compute result.
        for (int c = 0; c < depth; ++c) {
          output_data[Offset(output_dims, c, x, y, b)] =
              std::exp((input_data[Offset(input_dims, c, x, y, b)] - max) *
                       beta) /
              sum;
        }
      }
    }
  }
}

inline void Softmax(const uint8* input_data, const Dims<4>& input_dims,
                    int32 input_beta_multiplier, int32 input_beta_left_shift,
                    int diff_min, uint8* output_data,
                    const Dims<4>& output_dims) {
  // The representation chosen for the input to the exp() function is Q5.26.
  // We need to leave extra space since values that we skip might be as large as
  // -32 before multiplying by input_beta_multiplier, and therefore as large as
  // -16 afterwards.  Note that exp(-8) is definitely not insignificant to
  // accumulation, but exp(-16) definitely is.
  static const int kScaledDiffIntegerBits = 5;
  static const int kAccumulationIntegerBits = 12;
  using FixedPointScaledDiff =
      gemmlowp::FixedPoint<int32, kScaledDiffIntegerBits>;
  using FixedPointAccum = gemmlowp::FixedPoint<int32, kAccumulationIntegerBits>;
  using FixedPoint0 = gemmlowp::FixedPoint<int32, 0>;

  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);

  for (int b = 0; b < batches; ++b) {
    for (int x = 0; x < width; ++x) {
      for (int y = 0; y < height; ++y) {
        uint8 max_in_row = 0;
        for (int c = 0; c < depth; ++c) {
          max_in_row =
              std::max(max_in_row, input_data[Offset(input_dims, c, x, y, b)]);
        }

        FixedPointAccum sum_of_exps = FixedPointAccum::Zero();
        for (int c = 0; c < depth; ++c) {
          int32 input_diff =
              static_cast<int32>(input_data[Offset(input_dims, c, x, y, b)]) -
              max_in_row;
          if (input_diff >= diff_min) {
            const int32 input_diff_rescaled =
                MultiplyByQuantizedMultiplierGreaterThanOne(
                    input_diff, input_beta_multiplier, input_beta_left_shift);
            const FixedPointScaledDiff scaled_diff_f8 =
                FixedPointScaledDiff::FromRaw(input_diff_rescaled);
            sum_of_exps =
                sum_of_exps + gemmlowp::Rescale<kAccumulationIntegerBits>(
                                  exp_on_negative_values(scaled_diff_f8));
          }
        }

        int32 fixed_sum_of_exps = sum_of_exps.raw();
        int headroom_plus_one =
            CountLeadingZeros(static_cast<uint32>(fixed_sum_of_exps));
        // This is the number of bits to the left of the binary point above 1.0.
        // Consider fixed_sum_of_exps=1.25.  In that case shifted_scale=0.8 and
        // no later adjustment will be needed.
        int num_bits_over_unit = kAccumulationIntegerBits - headroom_plus_one;
        int32 shifted_sum_minus_one = static_cast<int32>(
            (static_cast<uint32>(fixed_sum_of_exps) << headroom_plus_one) -
            (static_cast<uint32>(1) << 31));

        FixedPoint0 shifted_scale = gemmlowp::one_over_one_plus_x_for_x_in_0_1(
            FixedPoint0::FromRaw(shifted_sum_minus_one));

        for (int c = 0; c < depth; ++c) {
          int32 input_diff =
              static_cast<int32>(input_data[Offset(input_dims, c, x, y, b)]) -
              max_in_row;
          if (input_diff >= diff_min) {
            const int32 input_diff_rescaled =
                MultiplyByQuantizedMultiplierGreaterThanOne(
                    input_diff, input_beta_multiplier, input_beta_left_shift);
            const FixedPointScaledDiff scaled_diff_f8 =
                FixedPointScaledDiff::FromRaw(input_diff_rescaled);

            FixedPoint0 exp_in_0 = exp_on_negative_values(scaled_diff_f8);
            int32 unsat_output = gemmlowp::RoundingDivideByPOT(
                (shifted_scale * exp_in_0).raw(), num_bits_over_unit + 31 - 8);

            output_data[Offset(output_dims, c, x, y, b)] = static_cast<uint8>(
                std::max(std::min(unsat_output, static_cast<int32>(255)), 0));

          } else {
            output_data[Offset(output_dims, c, x, y, b)] = 0;
          }
        }
      }
    }
  }
}

inline void Logistic(const float* input_data, const Dims<4>& input_dims,
                     float* output_data, const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          float val = input_data[Offset(input_dims, c, x, y, b)];
          float result = 1.f / (1.f + std::exp(-val));
          output_data[Offset(output_dims, c, x, y, b)] = result;
        }
      }
    }
  }
}

inline void Logistic(const uint8* input_data, const Dims<4>& input_dims,
                     int32 input_zero_point, int32 input_range_radius,
                     int32 input_multiplier, int input_left_shift,
                     uint8* output_data, const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          const uint8 input_val_u8 = input_data[Offset(input_dims, c, x, y, b)];
          const int32 input_val_centered =
              static_cast<int32>(input_val_u8) - input_zero_point;
          uint8 output_val;
          if (input_val_centered <= -input_range_radius) {
            output_val = 0;
          } else if (input_val_centered >= input_range_radius) {
            output_val = 255;
          } else {
            const int32 input_val_rescaled =
                MultiplyByQuantizedMultiplierGreaterThanOne(
                    input_val_centered, input_multiplier, input_left_shift);
            using FixedPoint4 = gemmlowp::FixedPoint<int32, 4>;
            using FixedPoint0 = gemmlowp::FixedPoint<int32, 0>;
            const FixedPoint4 input_val_f4 =
                FixedPoint4::FromRaw(input_val_rescaled);
            const FixedPoint0 output_val_f0 = gemmlowp::logistic(input_val_f4);
            using gemmlowp::RoundingDivideByPOT;
            int32 output_val_s32 = RoundingDivideByPOT(output_val_f0.raw(), 23);
            if (output_val_s32 == 256) {
              output_val_s32 = 255;
            }
            TFLITE_DCHECK_GE(output_val_s32, 0);
            TFLITE_DCHECK_LE(output_val_s32, 255);
            output_val = static_cast<uint8>(output_val_s32);
          }
          output_data[Offset(output_dims, c, x, y, b)] = output_val;
        }
      }
    }
  }
}

inline void Tanh(const float* input_data, const Dims<4>& input_dims,
                 float* output_data, const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          float val = input_data[Offset(input_dims, c, x, y, b)];
          float result = std::tanh(val);
          output_data[Offset(output_dims, c, x, y, b)] = result;
        }
      }
    }
  }
}

inline void Dequantize(const uint8* input_data, const Dims<4>& input_dims,
                       int32 zero_point, double scale, float* output_data,
                       const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          int32 val = input_data[Offset(input_dims, c, x, y, b)];
          float result = static_cast<float>(scale * (val - zero_point));
          output_data[Offset(output_dims, c, x, y, b)] = result;
        }
      }
    }
  }
}

inline void FakeQuant(const float* input_data, const Dims<4>& input_dims,
                      float rmin, float rmax, float* output_data,
                      const Dims<4>& output_dims) {
  // 0 should always be a representable value. Let's assume that the initial
  // min,max range contains 0.
  TFLITE_DCHECK_LE(rmin, 0.);
  TFLITE_DCHECK_GE(rmax, 0.);

  // Determine quantization parameters: zero_point, scale.
  using Integer = uint8;
  const Integer qmin = std::numeric_limits<Integer>::min();
  const Integer qmax = std::numeric_limits<Integer>::max();
  const float qmin_float = qmin;
  const float qmax_float = qmax;
  int32 zero_point = 0;
  float scale = 0.f;
  // If rmin==rmax, both must be zero per the above assertion,
  // so we are done.
  if (rmin != rmax) {
    // First determine the scale.
    scale = (rmax - rmin) / (qmax_float - qmin_float);

    // Zero-point computation.
    // First the initial floating-point computation. The zero-point can be
    // determined from solving an affine equation for any known pair
    // (real value, corresponding quantized value).
    // We know two such pairs: (rmin, qmin) and (rmax, qmax).
    // The arithmetic error on the zero point computed from either pair
    // will be roughly machine_epsilon * (sum of absolute values of terms)
    // so we want to use the variant that adds the smaller terms.
    const float zero_point_from_min = qmin_float - rmin / scale;
    const float zero_point_from_max = qmax_float - rmax / scale;
    const float zero_point_from_min_error =
        std::abs(qmin_float) + std::abs(rmin / scale);
    const float zero_point_from_max_error =
        std::abs(qmax_float) + std::abs(rmax / scale);

    const float zero_point_float =
        zero_point_from_min_error < zero_point_from_max_error
            ? zero_point_from_min
            : zero_point_from_max;

    // Now we need to nudge the zero point to be an integer
    // (our zero points are integer, and this is motivated by the requirement
    // to be able to represent the real value "0" exactly as a quantized value,
    // which is required in multiple places, for example in Im2col with SAME
    // padding).
    if (zero_point_float < qmin_float) {
      zero_point = qmin;
    } else if (zero_point_float > qmax_float) {
      zero_point = qmax;
    } else {
      zero_point = static_cast<int32>(TfLiteRound(zero_point_float));
    }
    // The zero point should always be in the range of quantized value,
    // [qmin, qmax].
    TFLITE_DCHECK_GE(zero_point, qmin);
    TFLITE_DCHECK_LE(zero_point, qmax);
  }

  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          const float src_val = input_data[Offset(input_dims, c, x, y, b)];
          const float unclamped_quantized_val =
              TfLiteRound(zero_point + src_val / scale);
          const float quantized_val = std::min(
              qmax_float, std::max(qmin_float, unclamped_quantized_val));
          const float dst_val = scale * (quantized_val - zero_point);
          output_data[Offset(output_dims, c, x, y, b)] = dst_val;
        }
      }
    }
  }
}

template <typename SrcT, typename DstT>
inline void Cast(const SrcT* input_data, const Dims<4>& input_dims,
                 DstT* output_data, const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          int offset = Offset(input_dims, c, x, y, b);
          output_data[offset] = static_cast<DstT>(input_data[offset]);
        }
      }
    }
  }
}

inline void Floor(const float* input_data, const Dims<4>& input_dims,
                  float* output_data, const Dims<4>& output_dims) {
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = MatchingArraySize(input_dims, 0, output_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        for (int c = 0; c < depth; ++c) {
          int offset = Offset(input_dims, c, x, y, b);
          output_data[offset] = std::floor(input_data[offset]);
        }
      }
    }
  }
}

template <typename T>
inline void Gather(const T* input_data, const Dims<4>& input_dims,
                   int input_rank, const int32* coords_data,
                   const Dims<4>& coords_dims, T* output_data,
                   const Dims<4>& output_dims) {
  TFLITE_DCHECK(coords_dims.sizes[0] == output_dims.sizes[input_rank - 1]);
  int stride = input_dims.strides[input_rank - 1];
  T* out = output_data;

  for (int i = 0; i < coords_dims.sizes[0]; i++) {
    TFLITE_DCHECK_GE(coords_data[i], 0);
    TFLITE_DCHECK_LT(coords_data[i], input_dims.sizes[input_rank - 1]);
    const T* in = input_data + coords_data[i] * stride;
    memcpy(out, in, sizeof(T) * stride);
    out += stride;
  }
}

inline void ResizeBilinear(const float* input_data, const Dims<4>& input_dims,
                           const int32* output_size_data,
                           const Dims<4>& output_size_dims, float* output_data,
                           const Dims<4>& output_dims) {
  int32 batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  int32 input_height = ArraySize(input_dims, 2);
  int32 input_width = ArraySize(input_dims, 1);
  int32 depth = MatchingArraySize(input_dims, 0, output_dims, 0);

  TFLITE_DCHECK_EQ(ArraySize(output_size_dims, 3), 1);
  TFLITE_DCHECK_EQ(ArraySize(output_size_dims, 2), 1);
  TFLITE_DCHECK_EQ(ArraySize(output_size_dims, 1), 1);
  TFLITE_DCHECK_EQ(ArraySize(output_size_dims, 0), 2);
  int32 output_height = output_size_data[Offset(output_size_dims, 0, 0, 0, 0)];
  int32 output_width = output_size_data[Offset(output_size_dims, 1, 0, 0, 0)];
  float height_scale = static_cast<float>(input_height) / output_height;
  float width_scale = static_cast<float>(input_width) / output_width;

  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < output_height; ++y) {
      float input_y = y * height_scale;
      int32 y0 = static_cast<int32>(std::floor(input_y));
      int32 y1 = std::min(y0 + 1, input_height - 1);
      for (int x = 0; x < output_width; ++x) {
        float input_x = x * width_scale;
        int32 x0 = static_cast<int32>(std::floor(input_x));
        int32 x1 = std::min(x0 + 1, input_width - 1);
        for (int c = 0; c < depth; ++c) {
          float interpolation = input_data[Offset(input_dims, c, x0, y0, b)] *
                                    (1 - (input_y - y0)) *
                                    (1 - (input_x - x0)) +
                                input_data[Offset(input_dims, c, x0, y1, b)] *
                                    (input_y - y0) * (1 - (input_x - x0)) +
                                input_data[Offset(input_dims, c, x1, y0, b)] *
                                    (1 - (input_y - y0)) * (input_x - x0) +
                                input_data[Offset(input_dims, c, x1, y1, b)] *
                                    (input_y - y0) * (input_x - x0);
          output_data[Offset(output_dims, c, x, y, b)] = interpolation;
        }
      }
    }
  }
}

template <typename T>
inline void SpaceToBatchND(const T* input_data, const Dims<4>& input_dims,
                           const int32* block_shape_data,
                           const Dims<4>& block_shape_dims,
                           const int32* paddings_data,
                           const Dims<4>& paddings_dims, T* output_data,
                           const Dims<4>& output_dims) {
  const int output_batch_size = ArraySize(output_dims, 3);
  const int output_height = ArraySize(output_dims, 2);
  const int output_width = ArraySize(output_dims, 1);
  const int input_batch_size = ArraySize(input_dims, 3);
  const int input_height = ArraySize(input_dims, 2);
  const int input_width = ArraySize(input_dims, 1);
  const int depth = ArraySize(input_dims, 0);
  const int block_shape_height = block_shape_data[0];
  const int block_shape_width = block_shape_data[1];
  const int padding_top = paddings_data[0];
  const int padding_left = paddings_data[2];

  for (int out_b = 0; out_b < output_batch_size; ++out_b) {
    int input_batch = out_b % input_batch_size;
    int shift_w = (out_b / input_batch_size) % block_shape_width;
    int shift_h = (out_b / input_batch_size) / block_shape_width;
    for (int out_h = 0; out_h < output_height; ++out_h) {
      for (int out_w = 0; out_w < output_width; ++out_w) {
        T* out = output_data + Offset(output_dims, 0, out_w, out_h, out_b);
        if (out_h * block_shape_height < padding_top ||
            out_h * block_shape_height >= padding_top + input_height ||
            out_w * block_shape_width < padding_left ||
            out_w * block_shape_width >= padding_left + input_width) {
          memset(out, 0, depth * sizeof(T));
        } else {
          const T* in =
              input_data +
              Offset(input_dims, 0,
                     (out_w * block_shape_width + shift_w) - padding_left,
                     (out_h * block_shape_height + shift_h) - padding_top,
                     input_batch);
          memcpy(out, in, depth * sizeof(T));
        }
      }
    }
  }
}

template <typename T>
inline void BatchToSpaceND(const T* input_data, const Dims<4>& input_dims,
                           const int32* block_shape_data,
                           const Dims<4>& block_shape_dims, T* output_data,
                           const Dims<4>& output_dims) {
  const int output_batch_size = ArraySize(output_dims, 3);
  const int input_batch_size = ArraySize(input_dims, 3);
  const int input_height = ArraySize(input_dims, 2);
  const int input_width = ArraySize(input_dims, 1);
  const int depth = ArraySize(input_dims, 0);
  const int block_shape_width = block_shape_data[1];
  const int block_shape_height = block_shape_data[0];

  for (int in_batch = 0; in_batch < input_batch_size; ++in_batch) {
    for (int in_h = 0; in_h < input_height; ++in_h) {
      for (int in_w = 0; in_w < input_width; ++in_w) {
        int out_batch = in_batch % output_batch_size;
        int out_w = in_w * block_shape_width +
                    (in_batch / output_batch_size) % block_shape_width;
        int out_h = in_h * block_shape_height +
                    (in_batch / output_batch_size) / block_shape_width;
        T* out = output_data + Offset(output_dims, 0, out_w, out_h, out_batch);
        const T* in = input_data + Offset(input_dims, 0, in_w, in_h, in_batch);
        memcpy(out, in, depth * sizeof(T));
      }
    }
  }
}

template <typename T>
inline void Pad(const T* input_data, const Dims<4>& input_dims,
                const std::vector<int>& left_paddings,
                const std::vector<int>& right_paddings, T* output_data,
                const Dims<4>& output_dims) {
  const int output_batch = ArraySize(output_dims, 3);
  const int output_height = ArraySize(output_dims, 2);
  const int output_width = ArraySize(output_dims, 1);
  const int output_depth = ArraySize(output_dims, 0);

  const int left_b_padding = left_paddings[3];
  const int left_h_padding = left_paddings[2];
  const int left_w_padding = left_paddings[1];
  const int left_d_padding = left_paddings[0];

  const int right_b_padding = right_paddings[3];
  const int right_h_padding = right_paddings[2];
  const int right_w_padding = right_paddings[1];
  const int right_d_padding = right_paddings[0];

  const T* in_ptr = input_data;
  T* out_ptr = output_data;
  for (int out_b = 0; out_b < output_batch; ++out_b) {
    for (int out_h = 0; out_h < output_height; ++out_h) {
      for (int out_w = 0; out_w < output_width; ++out_w) {
        for (int out_d = 0; out_d < output_depth; ++out_d) {
          if (out_b < left_b_padding ||
              out_b >= output_batch - right_b_padding ||
              out_h < left_h_padding ||
              out_h >= output_height - right_h_padding ||
              out_w < left_w_padding ||
              out_w >= output_width - right_w_padding ||
              out_d < left_d_padding ||
              out_d >= output_depth - right_d_padding) {
            *out_ptr++ = 0;
          } else {
            *out_ptr++ = *in_ptr++;
          }
        }
      }
    }
  }
}

template <typename T>
inline void StridedSlice(const T* input_data, const Dims<4>& input_dims,
                         int begin_mask, int end_mask,
                         const std::vector<int>& starts,
                         const std::vector<int>& stops,
                         const std::vector<int>& strides, T* output_data,
                         const Dims<4>& output_dims) {
  const int start_b = (begin_mask & 8) ? 0 : starts[3];
  const int stop_b = (end_mask & 8) ? input_dims.sizes[3] : stops[3];
  const int start_h = (begin_mask & 4) ? 0 : starts[2];
  const int stop_h = (end_mask & 4) ? input_dims.sizes[2] : stops[2];
  const int start_w = (begin_mask & 2) ? 0 : starts[1];
  const int stop_w = (end_mask & 2) ? input_dims.sizes[1] : stops[1];
  const int start_d = (begin_mask & 1) ? 0 : starts[0];
  const int stop_d = (end_mask & 1) ? input_dims.sizes[0] : stops[0];

  T* out_ptr = output_data;
  for (int in_b = start_b; in_b < stop_b; in_b += strides[3]) {
    for (int in_h = start_h; in_h < stop_h; in_h += strides[2]) {
      for (int in_w = start_w; in_w < stop_w; in_w += strides[1]) {
        for (int in_d = start_d; in_d < stop_d; in_d += strides[0]) {
          *out_ptr++ = input_data[Offset(input_dims, in_d, in_w, in_h, in_b)];
        }
      }
    }
  }
}

template <typename T>
inline void Slice(const T* input_data, const Dims<4>& input_dims,
                  const std::vector<int>& begin, const std::vector<int>& size,
                  T* output_data, const Dims<4>& output_dims) {
  // TODO(dkalenichenko): This op only supports 4D tensors.
  TFLITE_DCHECK_EQ(begin.size(), 4);
  TFLITE_DCHECK_EQ(size.size(), 4);
  const int start_b = begin[3];
  const int stop_b =
      size[3] == -1 ? input_dims.sizes[3] - start_b : start_b + size[3];
  const int start_h = begin[2];
  const int stop_h =
      size[2] == -1 ? input_dims.sizes[2] - start_b : start_b + size[2];
  const int start_w = begin[1];
  const int stop_w =
      size[1] == -1 ? input_dims.sizes[1] - start_b : start_b + size[1];
  const int start_d = begin[0];
  const int stop_d =
      size[0] == -1 ? input_dims.sizes[0] - start_d : start_d + size[0];

  T* out_ptr = output_data;
  for (int in_b = start_b; in_b < stop_b; ++in_b) {
    for (int in_h = start_h; in_h < stop_h; ++in_h) {
      for (int in_w = start_w; in_w < stop_w; ++in_w) {
        for (int in_d = start_d; in_d < stop_d; ++in_d) {
          *out_ptr++ = input_data[Offset(input_dims, in_d, in_w, in_h, in_b)];
        }
      }
    }
  }
}

template <typename T>
inline void Mean(const T* input_data, const Dims<4>& input_dims,
                 const std::vector<int>& reduction_indices, T* output_data,
                 const Dims<4>& output_dims) {
  const int output_batch = ArraySize(output_dims, 3);
  const int output_height = ArraySize(output_dims, 2);
  const int output_width = ArraySize(output_dims, 1);
  const int output_depth = ArraySize(output_dims, 0);

  const int input_height = ArraySize(input_dims, 2);
  const int input_width = ArraySize(input_dims, 1);

  // The current implementation only supports simultaneous reduction over
  // width and height.
  TFLITE_DCHECK_EQ(reduction_indices.size(), 2);
  TFLITE_DCHECK((reduction_indices[0] == 1 && reduction_indices[1] == 2) ||
                (reduction_indices[0] == 2 && reduction_indices[1] == 1));
  TFLITE_DCHECK_EQ(output_height, 1);
  TFLITE_DCHECK_EQ(output_width, 1);

  for (int out_b = 0; out_b < output_batch; ++out_b) {
    for (int out_d = 0; out_d < output_depth; ++out_d) {
      float value = 0;
      for (int in_h = 0; in_h < input_height; ++in_h) {
        for (int in_w = 0; in_w < input_width; ++in_w) {
          value += input_data[Offset(input_dims, out_d, in_w, in_h, out_b)];
        }
      }
      output_data[Offset(output_dims, out_d, 0, 0, out_b)] =
          value / (input_width * input_height);
    }
  }
}

template <typename T>
void Sub(const T* input1_data, const Dims<4>& input1_dims, const T* input2_data,
         const Dims<4>& input2_dims, T* output_data,
         const Dims<4>& output_dims) {
  NdArrayDesc<4> desc1;
  NdArrayDesc<4> desc2;
  NdArrayDescsForElementwiseBroadcast(input1_dims, input2_dims, &desc1, &desc2);

  // In Tensorflow, the dimensions are canonically named (batch_number, row,
  // col, channel), with extents (batches, height, width, depth), with the
  // trailing dimension changing most rapidly (channels has the smallest stride,
  // typically 1 element).
  //
  // In generated C code, we store arrays with the dimensions reversed. The
  // first dimension has smallest stride.
  //
  // We name our variables by their Tensorflow convention, but generate C code
  // nesting loops such that the innermost loop has the smallest stride for the
  // best cache behavior.
  for (int b = 0; b < ArraySize(output_dims, 3); ++b) {
    for (int y = 0; y < ArraySize(output_dims, 2); ++y) {
      for (int x = 0; x < ArraySize(output_dims, 1); ++x) {
        for (int c = 0; c < ArraySize(output_dims, 0); ++c) {
          output_data[Offset(output_dims, c, x, y, b)] =
              input1_data[SubscriptToIndex(desc1, c, x, y, b)] -
              input2_data[SubscriptToIndex(desc2, c, x, y, b)];
        }
      }
    }
  }
}

template <typename T>
void TensorFlowMinimum(const T* input1_data, const Dims<4>& input1_dims,
                       const T* input2_data, T* output_data,
                       const Dims<4>& output_dims) {
  int batches = MatchingArraySize(input1_dims, 3, output_dims, 3);
  int input_height = MatchingArraySize(input1_dims, 2, output_dims, 2);
  int input_width = MatchingArraySize(input1_dims, 1, output_dims, 1);
  int depth = MatchingArraySize(input1_dims, 0, output_dims, 0);

  auto min_value = input2_data[0];

  for (int b = 0; b < batches; b++) {
    for (int y = 0; y < input_height; y++) {
      for (int x = 0; x < input_width; x++) {
        for (int c = 0; c < depth; c++) {
          int offset = Offset(input1_dims, c, x, y, b);
          output_data[offset] =
              input1_data[offset] > min_value ? min_value : input1_data[offset];
        }
      }
    }
  }
}

template <typename T>
void TensorFlowMaximum(const T* input1_data, const Dims<4>& input1_dims,
                       const T* input2_data, T* output_data,
                       const Dims<4>& output_dims) {
  int batches = MatchingArraySize(input1_dims, 3, output_dims, 3);
  int input_height = MatchingArraySize(input1_dims, 2, output_dims, 2);
  int input_width = MatchingArraySize(input1_dims, 1, output_dims, 1);
  int depth = MatchingArraySize(input1_dims, 0, output_dims, 0);

  auto max_value = input2_data[0];

  for (int b = 0; b < batches; b++) {
    for (int y = 0; y < input_height; y++) {
      for (int x = 0; x < input_width; x++) {
        for (int c = 0; c < depth; c++) {
          int offset = Offset(input1_dims, c, x, y, b);
          output_data[offset] =
              input1_data[offset] < max_value ? max_value : input1_data[offset];
        }
      }
    }
  }
}

template <typename T1, typename T2, typename T3>
void ArgMax(const T3* axis, const T1* input_data, const Dims<4>& input_dims,
            T2* output_data, const Dims<4>& output_dims) {
  // The current ArgMax implemention can only determine the index of the maximum
  // value in the last dimension. So the axis argument is ignored.
  TFLITE_DCHECK_EQ(axis[0], 3);

  // For ArgMax, the number of output dimensions = (number of input dimensions -
  // 1). For the sake of simplicity, the output dimensions are equal to the
  // input dimensions here. We enforce the constraint that the last dimension
  // must always be 1.
  TFLITE_DCHECK_EQ(ArraySize(output_dims, 0), 1);
  const int batches = MatchingArraySize(input_dims, 3, output_dims, 3);
  const int height = MatchingArraySize(input_dims, 2, output_dims, 2);
  const int width = MatchingArraySize(input_dims, 1, output_dims, 1);
  const int depth = ArraySize(input_dims, 0);
  for (int b = 0; b < batches; ++b) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        auto max_value = input_data[Offset(input_dims, 0, x, y, b)];
        int max_index = 0;
        for (int d = 1; d < depth; ++d) {
          const auto& curr_value = input_data[Offset(input_dims, d, x, y, b)];
          if (curr_value > max_value) {
            max_value = curr_value;
            max_index = d;
          }
        }
        output_data[Offset(output_dims, 0, x, y, b)] = max_index;
      }
    }
  }
}

}  // namespace reference_ops
}  // namespace tflite

#endif  // THIRD_PARTY_TENSORFLOW_CONTRIB_LITE_KERNELS_INTERNAL_REFERENCE_REFERENCE_OPS_H_
