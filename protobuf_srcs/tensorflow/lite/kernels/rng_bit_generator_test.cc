/* Copyright 2023 The TensorFlow Authors. All Rights Reserved.

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

#include <cstdint>
#include <initializer_list>
#include <vector>

#include <gtest/gtest.h>
#include "tensorflow/lite/kernels/test_util.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace tflite {
namespace {

class RngBitGeneratorOpModel : public SingleOpModel {
 public:
  RngBitGeneratorOpModel(const tflite::RngAlgorithm algorithm,
                         const TensorData& initial_state,
                         const TensorData& output_state,
                         const TensorData& output) {
    initial_state_ = AddInput(initial_state);
    output_state_ = AddOutput(output_state);
    output_ = AddOutput(output);
    SetBuiltinOp(
        BuiltinOperator_STABLEHLO_RNG_BIT_GENERATOR,
        BuiltinOptions2_StablehloRngBitGeneratorOptions,
        CreateStablehloRngBitGeneratorOptions(builder_, algorithm).Union());
    BuildInterpreter({GetShape(initial_state_)});
  }

  template <typename T>
  void SetInitialState(std::initializer_list<T> data) {
    PopulateTensor<T>(initial_state_, data);
  }

  template <typename T>
  std::vector<T> GetOutputState() {
    return ExtractVector<T>(output_state_);
  }

  template <typename T>
  std::vector<T> GetOutput() {
    return ExtractVector<T>(output_);
  }

 protected:
  int initial_state_;
  int shape_;
  int output_state_;
  int output_;
};

template <typename output_integer_type>
void ValidateRngOutputAndOutputState(
    const tflite::RngAlgorithm algorithm,
    std::initializer_list<uint64_t> initial_state_val,
    std::vector<int> initial_state_shape,
    std::initializer_list<int32_t> output_shape,
    std::vector<uint64_t> expected_output_state,
    std::vector<output_integer_type> expected_output_val) {
  RngBitGeneratorOpModel m(
      algorithm,
      /*initial_state=*/{TensorType_UINT64, initial_state_shape},
      /*output_state=*/{TensorType_UINT64, initial_state_shape},
      /*output=*/{GetTensorType<output_integer_type>(), output_shape});
  m.SetInitialState<uint64_t>(initial_state_val);

  ASSERT_EQ(m.Invoke(), kTfLiteOk);
  std::vector<uint64_t> output_state = m.GetOutputState<uint64_t>();
  std::vector<output_integer_type> output = m.GetOutput<output_integer_type>();
  ASSERT_EQ(output_state, expected_output_state);
  ASSERT_EQ(output, expected_output_val);
}

// The expected values in the tests are generated by
// tensorflow/compiler/tests/xla_ops_test.py
TEST(RngBitGeneratorOpTest, PhiloxOutputInt32) {
  ValidateRngOutputAndOutputState<int32_t>(
      tflite::RngAlgorithm_PHILOX, /*initial_state_val=*/{1, 2, 3},
      /*initial_state_shape=*/{3}, /*output_shape=*/{2, 3},
      /*expected_output_state=*/{1, 4, 3}, /*expected_output_val=*/
      {-263854262, 1366700262, 495645701, -1243243882, 89414891, 1917262711});
}

TEST(RngBitGeneratorOpTest, PhiloxOutputUInt32) {
  ValidateRngOutputAndOutputState<uint32_t>(
      tflite::RngAlgorithm_PHILOX, /*initial_state_val=*/{1, 2, 3},
      /*initial_state_shape=*/{3}, /*output_shape=*/{2, 3},
      /*expected_output_state=*/{1, 4, 3}, /*expected_output_val=*/
      {4031113034, 1366700262, 495645701, 3051723414, 89414891, 1917262711});
}

TEST(RngBitGeneratorOpTest, PhiloxOutputInt64) {
  ValidateRngOutputAndOutputState<int64_t>(
      tflite::RngAlgorithm_PHILOX, /*initial_state_val=*/{1, 2, 3},
      /*initial_state_shape=*/{3}, /*output_shape=*/{2, 3},
      /*expected_output_state=*/{1, 5, 3}, /*expected_output_val=*/
      {5869932932755744586, -5339691813646437371, 8234580641674714347,
       2641225993340350124, 1962472297844690804, -3580856229565614135});
}

TEST(RngBitGeneratorOpTest, PhiloxOutputUInt64) {
  ValidateRngOutputAndOutputState<uint64_t>(
      tflite::RngAlgorithm_PHILOX, /*initial_state_val=*/{1, 2, 3},
      /*initial_state_shape=*/{3}, /*output_shape=*/{2, 3},
      /*expected_output_state=*/{1, 5, 3}, /*expected_output_val=*/
      {5869932932755744586u, 13107052260063114245u, 8234580641674714347u,
       2641225993340350124u, 1962472297844690804u, 14865887844143937481u});
}

TEST(RngBitGeneratorOpTest, ThreefryOutputInt32) {
  ValidateRngOutputAndOutputState<int32_t>(
      tflite::RngAlgorithm_THREEFRY, /*initial_state_val=*/{1, 2},
      /*initial_state_shape=*/{2}, /*output_shape=*/{2, 3},
      /*expected_output_state=*/{1, 5}, /*expected_output_val=*/
      {43444564, -2144348869, -315321645, -549236733, 1672743891, -54463903});
}

TEST(RngBitGeneratorOpTest, ThreefryOutputUInt32) {
  ValidateRngOutputAndOutputState<uint32_t>(
      tflite::RngAlgorithm_THREEFRY, /*initial_state_val=*/{1, 2},
      /*initial_state_shape=*/{2}, /*output_shape=*/{2, 3},
      /*expected_output_state=*/{1, 5}, /*expected_output_val=*/
      {43444564, 2150618427, 3979645651, 3745730563, 1672743891, 4240503393});
}

TEST(RngBitGeneratorOpTest, ThreefryOutputInt64) {
  ValidateRngOutputAndOutputState<int64_t>(
      tflite::RngAlgorithm_THREEFRY, /*initial_state_val=*/{1, 2},
      /*initial_state_shape=*/{2}, /*output_shape=*/{2, 3},
      /*expected_output_state=*/{1, 8}, /*expected_output_val=*/
      {-9209908263526143660, -2358953802017238317, -233920680524772397,
       2658481902456610144, -2022031683723149139, -2324041912354448873});
}

TEST(RngBitGeneratorOpTest, ThreefryOutputUInt64) {
  ValidateRngOutputAndOutputState<uint64_t>(
      tflite::RngAlgorithm_THREEFRY, /*initial_state_val=*/{1, 2},
      /*initial_state_shape=*/{2}, /*output_shape=*/{2, 3},
      /*expected_output_state=*/{1, 8}, /*expected_output_val=*/
      {9236835810183407956u, 16087790271692313299u, 18212823393184779219u,
       2658481902456610144u, 16424712389986402477u, 16122702161355102743u});
}

TEST(RngBitGeneratorOpTest, DefaultOutputInt32) {
  ValidateRngOutputAndOutputState<int32_t>(
      tflite::RngAlgorithm_DEFAULT, /*initial_state_val=*/{1, 2, 3},
      /*initial_state_shape=*/{3}, /*output_shape=*/{2, 3},
      /*expected_output_state=*/{1, 4, 3}, /*expected_output_val=*/
      {-263854262, 1366700262, 495645701, -1243243882, 89414891, 1917262711});
}

TEST(RngBitGeneratorOpTest, DefaultOutputUInt32) {
  ValidateRngOutputAndOutputState<uint32_t>(
      tflite::RngAlgorithm_DEFAULT, /*initial_state_val=*/{1, 2, 3},
      /*initial_state_shape=*/{3}, /*output_shape=*/{2, 3},
      /*expected_output_state=*/{1, 4, 3}, /*expected_output_val=*/
      {4031113034, 1366700262, 495645701, 3051723414, 89414891, 1917262711});
}

TEST(RngBitGeneratorOpTest, DefaultOutputInt64) {
  ValidateRngOutputAndOutputState<int64_t>(
      tflite::RngAlgorithm_DEFAULT, /*initial_state_val=*/{1, 2, 3},
      /*initial_state_shape=*/{3}, /*output_shape=*/{2, 3},
      /*expected_output_state=*/{1, 5, 3}, /*expected_output_val=*/
      {5869932932755744586, -5339691813646437371, 8234580641674714347,
       2641225993340350124, 1962472297844690804, -3580856229565614135});
}

TEST(RngBitGeneratorOpTest, DefaultOutputUInt64) {
  ValidateRngOutputAndOutputState<uint64_t>(
      tflite::RngAlgorithm_DEFAULT, /*initial_state_val=*/{1, 2, 3},
      /*initial_state_shape=*/{3}, /*output_shape=*/{2, 3},
      /*expected_output_state=*/{1, 5, 3}, /*expected_output_val=*/
      {5869932932755744586u, 13107052260063114245u, 8234580641674714347u,
       2641225993340350124u, 1962472297844690804u, 14865887844143937481u});
}

template <typename output_integer_type>
void OutputIsDeterministicWithSameInitState(
    const tflite::RngAlgorithm algorithm,
    std::initializer_list<uint64_t> initial_state_val,
    std::vector<int> initial_state_shape) {
  RngBitGeneratorOpModel m1(
      algorithm,
      /*initial_state=*/{TensorType_UINT64, initial_state_shape},
      /*output_state=*/{TensorType_UINT64, initial_state_shape},
      /*output=*/{GetTensorType<output_integer_type>(), {3, 3}});
  RngBitGeneratorOpModel m2(
      algorithm,
      /*initial_state=*/{TensorType_UINT64, initial_state_shape},
      /*output_state=*/{TensorType_UINT64, initial_state_shape},
      /*output=*/{GetTensorType<output_integer_type>(), {3, 3}});
  m1.SetInitialState<uint64_t>(initial_state_val);
  m2.SetInitialState<uint64_t>(initial_state_val);

  ASSERT_EQ(m1.Invoke(), kTfLiteOk);
  std::vector<uint64_t> output_state_1a = m1.GetOutputState<uint64_t>();
  std::vector<output_integer_type> output_1a =
      m1.GetOutput<output_integer_type>();
  ASSERT_EQ(m1.Invoke(), kTfLiteOk);
  std::vector<uint64_t> output_state_1b = m1.GetOutputState<uint64_t>();
  std::vector<output_integer_type> output_1b =
      m1.GetOutput<output_integer_type>();
  ASSERT_EQ(output_state_1a, output_state_1b);
  ASSERT_EQ(output_1a, output_1b);

  ASSERT_EQ(m2.Invoke(), kTfLiteOk);
  std::vector<uint64_t> output_state_2 = m2.GetOutputState<uint64_t>();
  std::vector<output_integer_type> output_2 =
      m2.GetOutput<output_integer_type>();
  ASSERT_EQ(output_state_1a, output_state_2);
  ASSERT_EQ(output_1a, output_2);
}

TEST(RngBitGeneratorOpTest, PhiloxDeterministicOutputInt32) {
  OutputIsDeterministicWithSameInitState<int32_t>(
      tflite::RngAlgorithm_PHILOX, /*initial_state_val=*/{1, 2, 3},
      /*initial_state_shape=*/{3});
  OutputIsDeterministicWithSameInitState<int32_t>(tflite::RngAlgorithm_DEFAULT,
                                                  /*initial_state_val=*/{1, 2},
                                                  /*initial_state_shape=*/{2});
}

TEST(RngBitGeneratorOpTest, ThreefryDeterministicOutputInt32) {
  OutputIsDeterministicWithSameInitState<int32_t>(tflite::RngAlgorithm_THREEFRY,
                                                  /*initial_state_val=*/{1, 2},
                                                  /*initial_state_shape=*/{2});
}

TEST(RngBitGeneratorOpTest, PhiloxDeterministicOutputUInt32) {
  OutputIsDeterministicWithSameInitState<uint32_t>(
      tflite::RngAlgorithm_PHILOX, /*initial_state_val=*/{1, 2, 3},
      /*initial_state_shape=*/{3});
  OutputIsDeterministicWithSameInitState<uint32_t>(tflite::RngAlgorithm_DEFAULT,
                                                   /*initial_state_val=*/{1, 2},
                                                   /*initial_state_shape=*/{2});
}

TEST(RngBitGeneratorOpTest, ThreefryDeterministicOutputUInt32) {
  OutputIsDeterministicWithSameInitState<uint32_t>(
      tflite::RngAlgorithm_THREEFRY,
      /*initial_state_val=*/{1, 2},
      /*initial_state_shape=*/{2});
}

TEST(RngBitGeneratorOpTest, PhiloxDeterministicOutputInt64) {
  OutputIsDeterministicWithSameInitState<int64_t>(
      tflite::RngAlgorithm_PHILOX, /*initial_state_val=*/{1, 2, 3},
      /*initial_state_shape=*/{3});
  OutputIsDeterministicWithSameInitState<int64_t>(tflite::RngAlgorithm_DEFAULT,
                                                  /*initial_state_val=*/{1, 2},
                                                  /*initial_state_shape=*/{2});
}

TEST(RngBitGeneratorOpTest, ThreefryDeterministicOutputInt64) {
  OutputIsDeterministicWithSameInitState<int64_t>(tflite::RngAlgorithm_THREEFRY,
                                                  /*initial_state_val=*/{1, 2},
                                                  /*initial_state_shape=*/{2});
}

TEST(RngBitGeneratorOpTest, PhiloxDeterministicOutputUInt64) {
  OutputIsDeterministicWithSameInitState<uint64_t>(
      tflite::RngAlgorithm_PHILOX, /*initial_state_val=*/{1, 2, 3},
      /*initial_state_shape=*/{3});
  OutputIsDeterministicWithSameInitState<uint64_t>(tflite::RngAlgorithm_DEFAULT,
                                                   /*initial_state_val=*/{1, 2},
                                                   /*initial_state_shape=*/{2});
}

TEST(RngBitGeneratorOpTest, ThreefryDeterministicOutputUInt64) {
  OutputIsDeterministicWithSameInitState<uint64_t>(
      tflite::RngAlgorithm_THREEFRY,
      /*initial_state_val=*/{1, 2},
      /*initial_state_shape=*/{2});
}

}  // namespace
}  // namespace tflite
