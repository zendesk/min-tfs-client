/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/core/kernels/batching_util/batch_resource_base.h"

#include <cstdint>
#include <memory>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "tensorflow/core/common_runtime/cost_measurement.h"
#include "tensorflow/core/common_runtime/cost_measurement_registry.h"
#include "tensorflow/core/common_runtime/request_cost.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/types.pb.h"

namespace tensorflow {
namespace serving {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

class TestTpuCostMeasurement : public CostMeasurement {
 public:
  using CostMeasurement::CostMeasurement;

  absl::Duration GetTotalCost() override { return absl::Milliseconds(100); }
  absl::string_view GetCostType() const override { return "test_tpu"; }
};
REGISTER_COST_MEASUREMENT("test_tpu", TestTpuCostMeasurement);

class TestGcuCostMeasurement : public CostMeasurement {
 public:
  using CostMeasurement::CostMeasurement;

  absl::Duration GetTotalCost() override { return absl::Milliseconds(200); }
  absl::string_view GetCostType() const override { return "test_gcu"; }
};
REGISTER_COST_MEASUREMENT("test_gcu", TestGcuCostMeasurement);

std::unique_ptr<BatchResourceBase::BatchTask> MakeBatchTask(
    const int64_t task_size, RequestCost* request_cost) {
  auto task = std::make_unique<BatchResourceBase::BatchTask>();
  task->inputs.push_back(Tensor(DT_DOUBLE, TensorShape({task_size, 1})));
  task->request_cost = request_cost;
  return task;
}

TEST(SplitBatchCostsAndRecordMetricsTest, SkipOnNoCostMeasurement) {
  BatchResourceBase::BatchT batch;
  RequestCost cost;
  batch.AddTask(MakeBatchTask(/*task_size=*/1, &cost));
  batch.Close();

  std::vector<std::unique_ptr<CostMeasurement>> batch_cost_measurements;
  BatchResourceBase::SplitBatchCostsAndRecordMetrics(batch_cost_measurements,
                                                     /*processed_size=*/16,
                                                     batch);
  EXPECT_TRUE(batch.task(0).request_cost->GetCosts().empty());
  EXPECT_THAT(
      batch.task(0).request_cost->GetBatchMetrics(),
      ::testing::ElementsAre(::testing::FieldsAre(
          /*processed_size=*/16, /*input_size=*/1, /*padding_size=*/15)));
}

TEST(SplitBatchCostsAndRecordMetricsTest, SkipOnZeroCost) {
  BatchResourceBase::BatchT batch;
  RequestCost cost;
  batch.AddTask(MakeBatchTask(/*task_size=*/1, &cost));
  batch.Close();

  CostMeasurement::Context context{/*is_per_query=*/false};
  std::vector<std::unique_ptr<CostMeasurement>> batch_cost_measurements;
  batch_cost_measurements.push_back(
      CostMeasurementRegistry::CreateByNameOrNull("no_op", context));
  BatchResourceBase::SplitBatchCostsAndRecordMetrics(batch_cost_measurements,
                                                     /*processed_size=*/16,
                                                     batch);
  EXPECT_TRUE(batch.task(0).request_cost->GetCosts().empty());
  EXPECT_THAT(
      batch.task(0).request_cost->GetBatchMetrics(),
      ::testing::ElementsAre(::testing::FieldsAre(
          /*processed_size=*/16, /*input_size=*/1, /*padding_size=*/15)));
}

TEST(SplitBatchCostsAndRecordMetricsTest, SkipOnZeroBatchSize) {
  BatchResourceBase::BatchT batch;
  batch.Close();

  CostMeasurement::Context context{/*is_per_query=*/false};
  std::vector<std::unique_ptr<CostMeasurement>> batch_cost_measurements;
  batch_cost_measurements.push_back(
      CostMeasurementRegistry::CreateByNameOrNull("test_tpu", context));
  BatchResourceBase::SplitBatchCostsAndRecordMetrics(batch_cost_measurements,
                                                     /*processed_size=*/0,
                                                     batch);
}

TEST(SplitBatchCostsAndRecordMetricsTest, SkipOnNoRequestCost) {
  BatchResourceBase::BatchT batch;
  batch.AddTask(MakeBatchTask(/*task_size=*/1, nullptr));
  batch.AddTask(MakeBatchTask(/*task_size=*/9, nullptr));
  batch.Close();

  CostMeasurement::Context context{/*is_per_query=*/false};
  std::vector<std::unique_ptr<CostMeasurement>> batch_cost_measurements;
  batch_cost_measurements.push_back(
      CostMeasurementRegistry::CreateByNameOrNull("test_tpu", context));
  BatchResourceBase::SplitBatchCostsAndRecordMetrics(batch_cost_measurements,
                                                     /*processed_size=*/16,
                                                     batch);

  EXPECT_EQ(batch.task(0).request_cost, nullptr);
  EXPECT_EQ(batch.task(1).request_cost, nullptr);
}

TEST(SplitBatchCostsAndRecordMetricsTest, SplitSingleCostType) {
  BatchResourceBase::BatchT batch;
  RequestCost cost1, cost2;
  batch.AddTask(MakeBatchTask(/*task_size=*/1, &cost1));
  batch.AddTask(MakeBatchTask(/*task_size=*/9, &cost2));
  batch.Close();

  CostMeasurement::Context context{/*is_per_query=*/false};
  std::vector<std::unique_ptr<CostMeasurement>> batch_cost_measurements;
  batch_cost_measurements.push_back(
      CostMeasurementRegistry::CreateByNameOrNull("test_tpu", context));
  BatchResourceBase::SplitBatchCostsAndRecordMetrics(batch_cost_measurements,
                                                     /*processed_size=*/20,
                                                     batch);

  EXPECT_THAT(
      batch.task(0).request_cost->GetCosts(),
      UnorderedElementsAre(Pair("test_tpu_with_smear", absl::Milliseconds(10)),
                           Pair("test_tpu_no_smear", absl::Milliseconds(5))));
  EXPECT_THAT(
      batch.task(0).request_cost->GetBatchMetrics(),
      ::testing::ElementsAre(::testing::FieldsAre(
          /*processed_size=*/20, /*input_size=*/1, /*padding_size=*/10)));
  EXPECT_THAT(
      batch.task(1).request_cost->GetCosts(),
      UnorderedElementsAre(Pair("test_tpu_with_smear", absl::Milliseconds(90)),
                           Pair("test_tpu_no_smear", absl::Milliseconds(45))));
  EXPECT_THAT(
      batch.task(1).request_cost->GetBatchMetrics(),
      ::testing::ElementsAre(::testing::FieldsAre(
          /*processed_size=*/20, /*input_size=*/9, /*padding_size=*/10)));
}

TEST(SplitBatchCostsAndRecordMetricsTest, SplitMultiCostTypes) {
  BatchResourceBase::BatchT batch;
  RequestCost cost1, cost2;
  batch.AddTask(MakeBatchTask(/*task_size=*/1, &cost1));
  batch.AddTask(MakeBatchTask(/*task_size=*/9, &cost2));
  batch.Close();

  CostMeasurement::Context context{/*is_per_query=*/false};
  std::vector<std::unique_ptr<CostMeasurement>> batch_cost_measurements;
  batch_cost_measurements.push_back(
      CostMeasurementRegistry::CreateByNameOrNull("test_tpu", context));
  batch_cost_measurements.push_back(
      CostMeasurementRegistry::CreateByNameOrNull("test_gcu", context));
  BatchResourceBase::SplitBatchCostsAndRecordMetrics(batch_cost_measurements,
                                                     /*processed_size=*/20,
                                                     batch);

  EXPECT_THAT(
      batch.task(0).request_cost->GetCosts(),
      UnorderedElementsAre(Pair("test_tpu_with_smear", absl::Milliseconds(10)),
                           Pair("test_tpu_no_smear", absl::Milliseconds(5)),
                           Pair("test_gcu_with_smear", absl::Milliseconds(20)),
                           Pair("test_gcu_no_smear", absl::Milliseconds(10))));
  EXPECT_THAT(
      batch.task(0).request_cost->GetBatchMetrics(),
      ::testing::ElementsAre(::testing::FieldsAre(
          /*processed_size=*/20, /*input_size=*/1, /*padding_size=*/10)));

  EXPECT_THAT(
      batch.task(1).request_cost->GetCosts(),
      UnorderedElementsAre(Pair("test_tpu_with_smear", absl::Milliseconds(90)),
                           Pair("test_tpu_no_smear", absl::Milliseconds(45)),
                           Pair("test_gcu_with_smear", absl::Milliseconds(180)),
                           Pair("test_gcu_no_smear", absl::Milliseconds(90))));
  EXPECT_THAT(
      batch.task(1).request_cost->GetBatchMetrics(),
      ::testing::ElementsAre(::testing::FieldsAre(
          /*processed_size=*/20, /*input_size=*/9, /*padding_size=*/10)));
}

TEST(SplitBatchCostsAndRecordMetricsTest, SplitOnlyNonZeroCostTypes) {
  BatchResourceBase::BatchT batch;
  RequestCost cost1, cost2;
  batch.AddTask(MakeBatchTask(/*task_size=*/1, &cost1));
  batch.AddTask(MakeBatchTask(/*task_size=*/9, &cost2));
  batch.Close();

  CostMeasurement::Context context{/*is_per_query=*/false};
  std::vector<std::unique_ptr<CostMeasurement>> batch_cost_measurements;
  batch_cost_measurements.push_back(
      CostMeasurementRegistry::CreateByNameOrNull("no_op", context));
  batch_cost_measurements.push_back(
      CostMeasurementRegistry::CreateByNameOrNull("test_tpu", context));
  BatchResourceBase::SplitBatchCostsAndRecordMetrics(batch_cost_measurements,
                                                     /*processed_size=*/20,
                                                     batch);

  EXPECT_THAT(
      batch.task(0).request_cost->GetCosts(),
      UnorderedElementsAre(Pair("test_tpu_with_smear", absl::Milliseconds(10)),
                           Pair("test_tpu_no_smear", absl::Milliseconds(5))));
  EXPECT_THAT(
      batch.task(0).request_cost->GetBatchMetrics(),
      ::testing::ElementsAre(::testing::FieldsAre(
          /*processed_size=*/20, /*input_size=*/1, /*padding_size=*/10)));

  EXPECT_THAT(
      batch.task(1).request_cost->GetCosts(),
      UnorderedElementsAre(Pair("test_tpu_with_smear", absl::Milliseconds(90)),
                           Pair("test_tpu_no_smear", absl::Milliseconds(45))));
  EXPECT_THAT(
      batch.task(1).request_cost->GetBatchMetrics(),
      ::testing::ElementsAre(::testing::FieldsAre(
          /*processed_size=*/20, /*input_size=*/9, /*padding_size=*/10)));
}

}  // namespace
}  // namespace serving
}  // namespace tensorflow
