/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

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
#ifndef TENSORFLOW_CONTRIB_LITE_TOCO_GRAPH_TRANSFORMATIONS_QUANTIZATION_UTIL_H_
#define TENSORFLOW_CONTRIB_LITE_TOCO_GRAPH_TRANSFORMATIONS_QUANTIZATION_UTIL_H_

#include "tensorflow/contrib/lite/kernels/internal/quantization_util.h"
#include "tensorflow/contrib/lite/toco/graph_transformations/graph_transformations.h"
#include "tensorflow/contrib/lite/toco/model.h"

namespace toco {

// Gets the target quantized data type of an array based on the fake quant op.
// For example, if the num_bits is 8 the data type will be kUint8.
bool InferQuantizedDataTypeFromFakeQuant(
    const FakeQuantOperator& op, ArrayDataType* out_quantized_data_type);

// Gets the min/max numerical range for the given quantized data type.
// For example, kUint8 will return [0,255].
// Returns true if the ranges were set and false if the type is not quantized.
bool GetQuantizedDataTypeNumericalRange(ArrayDataType data_type,
                                        double* out_min_value,
                                        double* out_max_value);

// Returns the quantized data type of an array, falling back to the provided
// default data type.
ArrayDataType GetQuantizedDataType(const Array& array,
                                   ArrayDataType default_type);

// Returns the quantization params for the array with the given data type and
// minmax.
void GetQuantizationParams(ArrayDataType data_type, const MinMax& minmax,
                           QuantizationParams* quantization_params);

// Returns the quantization params for the data type and minmax values.
template <ArrayDataType A>
void GetQuantizationParamsFromMinMax(const MinMax& minmax,
                                     QuantizationParams* quantization_params) {
  using Integer = DataType<A>;
  const double rmin = minmax.min;
  const double rmax = minmax.max;
  *quantization_params =
      ::tflite::ChooseQuantizationParams<Integer>(rmin, rmax);
}

// Quantizes an array by setting its data type and (if constant) quantizing
// all values in the array.
void QuantizeArray(GraphTransformation* transformation, Model* model,
                   const string& name, ArrayDataType quantized_data_type,
                   const QuantizationParams& quantization_params);

// Returns true if the given array, when quantized, contains only values between
// the provided clamp min/max.
// Either clamp_min or clamp_max may be +/-infinity to indicate that the value
// is unbounded on that side.
bool IsArrayQuantizedRangeSubset(GraphTransformation* transformation,
                                 const Array& array, double clamp_min,
                                 double clamp_max);

}  // namespace toco

#endif  // TENSORFLOW_CONTRIB_LITE_TOCO_GRAPH_TRANSFORMATIONS_QUANTIZATION_UTIL_H_
