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

#include "tensorflow/lite/c/builtin_op_data.h"
#include "tensorflow/lite/c/c_api_internal.h"
#include "tensorflow/lite/experimental/micro/kernels/all_ops_resolver.h"
#include "tensorflow/lite/experimental/micro/testing/micro_test.h"
#include "tensorflow/lite/experimental/micro/testing/test_utils.h"

namespace tflite {
namespace testing {
namespace {

void TestRound(std::initializer_list<int> input_dims_data,
               std::initializer_list<float> input_data,
               std::initializer_list<float> expected_output_data,
               float* output_data) {
  TfLiteIntArray* input_dims = IntArrayFromInitializer(input_dims_data);
  TfLiteIntArray* output_dims = IntArrayFromInitializer(input_dims_data);
  const int output_dims_count = ElementCount(*output_dims);
  constexpr int inputs_size = 1;
  constexpr int outputs_size = 1;
  constexpr int tensors_size = inputs_size + outputs_size;
  TfLiteTensor tensors[tensors_size] = {
      CreateFloatTensor(input_data, input_dims, "input_tensor"),
      CreateFloatTensor(output_data, output_dims, "output_tensor"),
  };
  TfLiteContext context;
  PopulateContext(tensors, tensors_size, &context);
  ::tflite::ops::micro::AllOpsResolver resolver;
  const TfLiteRegistration* registration =
      resolver.FindOp(tflite::BuiltinOperator_ROUND, 1);
  TF_LITE_MICRO_EXPECT_NE(nullptr, registration);

  int inputs_array_data[] = {1, 0};
  TfLiteIntArray* inputs_array = IntArrayFromInts(inputs_array_data);
  int outputs_array_data[] = {1, 1};
  TfLiteIntArray* outputs_array = IntArrayFromInts(outputs_array_data);
  TfLiteIntArray* temporaries_array = IntArrayFromInitializer({0});
  TfLiteNode node;
  node.inputs = inputs_array;
  node.outputs = outputs_array;
  node.temporaries = temporaries_array;
  node.user_data = nullptr;
  node.builtin_data = nullptr;
  node.custom_initial_data = nullptr;
  node.custom_initial_data_size = 0;
  node.delegate = nullptr;
  TF_LITE_MICRO_EXPECT_NE(nullptr, registration->invoke);
  TF_LITE_MICRO_EXPECT_EQ(kTfLiteOk, registration->invoke(&context, &node));
  for (int i = 0; i < output_dims_count; ++i) {
    TF_LITE_MICRO_EXPECT_NEAR(expected_output_data.begin()[i], output_data[i],
                              1e-5f);
  }
}

}  // namespace
}  // namespace testing
}  // namespace tflite

TF_LITE_MICRO_TESTS_BEGIN

TF_LITE_MICRO_TEST(SingleDim) {
  float output_data[6];
  tflite::testing::TestRound({1, 6},  // input_dims_data
                             {8.5, 0.0, 3.5, 4.2, -3.5, -4.5},  // input_data
                             {8, 0, 4, 4, -4, -4},  // expected_output_data
                             output_data);
}

TF_LITE_MICRO_TEST(MultiDims) {
  float output_data[12];
  tflite::testing::TestRound(
      {4, 2, 1, 1, 6},  // input_dims_data
      {0.0001, 8.0001, 0.9999, 9.9999, 0.5, -0.0001, -8.0001, -0.9999, -9.9999,
       -0.5, -2.5, 1.5},                            // input_data
      {0, 8, 1, 10, 0, 0, -8, -1, -10, -0, -2, 2},  // expected_output_data
      output_data);
}

TF_LITE_MICRO_TESTS_END
