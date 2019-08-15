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
#include "tensorflow/core/kernels/data/experimental/random_dataset_op.h"

#include "tensorflow/core/kernels/data/dataset_test_base.h"
#include "tensorflow/core/lib/random/philox_random.h"
#include "tensorflow/core/lib/random/random.h"
#include "tensorflow/core/lib/random/random_distributions.h"

namespace tensorflow {
namespace data {
namespace experimental {
namespace {

constexpr char kNodeName[] = "sampling_dataset";
constexpr char kIteratorPrefix[] = "Iterator";

// Number of random samples generated per test
constexpr int kCount = 10;

// Generate the first `count` random numbers that the kernel should produce
// for a given seed/seed2 combo.
// For compatibility with the test harness, return value is a vector of scalar
// Tensors.
std::vector<Tensor> GenerateExpectedData(int64 seed, int64 seed2, int count) {
  std::vector<Tensor> ret;
  auto parent_generator = random::PhiloxRandom(seed, seed2);
  auto generator =
      random::SingleSampleAdapter<random::PhiloxRandom>(&parent_generator);

  for (int i = 0; i < count; ++i) {
    ret.push_back(CreateTensor<int64>(TensorShape({}), {generator()}));
  }
  return ret;
}

class RandomDatasetParams : public DatasetParams {
 public:
  RandomDatasetParams(int64 seed, int64 seed2, DataTypeVector output_dtypes,
                      std::vector<PartialTensorShape> output_shapes,
                      string node_name)
      : DatasetParams(std::move(output_dtypes), std::move(output_shapes),
                      std::move(node_name)),
        seed(CreateTensor<int64>(TensorShape({}), {seed})),
        seed2(CreateTensor<int64>(TensorShape({}), {seed2})) {}

  Status MakeInputs(gtl::InlinedVector<TensorValue, 4>* inputs) override {
    *inputs = {TensorValue(&seed), TensorValue(&seed2)};
    return Status::OK();
  }

 private:
  Tensor seed;
  Tensor seed2;
};

class RandomDatasetOpTest : public DatasetOpsTestBaseV2<RandomDatasetParams> {
 public:
  Status Initialize(RandomDatasetParams* dataset_params) override {
    // Step 1: Set up enough of a TF runtime to be able to invoke a kernel.
    TF_RETURN_IF_ERROR(InitThreadPool(thread_num_));
    TF_RETURN_IF_ERROR(InitFunctionLibraryRuntime({}, cpu_num_));

    // Step 2: Box up the four inputs to the kernel inside TensorValue objects
    // inside a vector.
    gtl::InlinedVector<TensorValue, 4> inputs;
    TF_RETURN_IF_ERROR(dataset_params->MakeInputs(&inputs));

    // Step 3: Create a dataset kernel to test, passing in attributes of the
    // kernel.
    TF_RETURN_IF_ERROR(
        CreateRandomDatasetOpKernel(*dataset_params, &dataset_kernel_));

    // Step 4: Create a context in which the kernel will operate. This is where
    // the kernel gets initialized with its inputs
    TF_RETURN_IF_ERROR(
        CreateDatasetContext(dataset_kernel_.get(), &inputs, &dataset_ctx_));

    // Step 5: Unbox the DatasetBase object inside the variant tensor backing
    // the kernel.
    TF_RETURN_IF_ERROR(
        CreateDataset(dataset_kernel_.get(), dataset_ctx_.get(), &dataset_));

    // Step 6: Create an iterator in case the test needs to read the output of
    // the dataset.
    TF_RETURN_IF_ERROR(
        CreateIteratorContext(dataset_ctx_.get(), &iterator_ctx_));
    TF_RETURN_IF_ERROR(dataset_->MakeIterator(iterator_ctx_.get(),
                                              kIteratorPrefix, &iterator_));

    return Status::OK();
  }

 protected:
  // Creates a new `RandomDataset` op kernel.
  // Doesn't initialize the random seeds because they are inputs, not
  // attributes.
  Status CreateRandomDatasetOpKernel(const RandomDatasetParams& dataset_params,
                                     std::unique_ptr<OpKernel>* op_kernel) {
    NodeDef node_def = test::function::NDef(
        kNodeName, name_utils::OpName(RandomDatasetOp::kDatasetType),
        // Inputs
        {RandomDatasetOp::kSeed, RandomDatasetOp::kSeed2},
        // Attributes
        {{RandomDatasetOp::kOutputTypes, dataset_params.output_dtypes},
         {RandomDatasetOp::kOutputShapes, dataset_params.output_shapes}});
    TF_RETURN_IF_ERROR(CreateOpKernel(node_def, op_kernel));
    return Status::OK();
  }
};

RandomDatasetParams FortyTwo() {
  return {/*seed=*/42,
          /*seed2=*/42,
          /*output_dtypes=*/{DT_INT64},
          /*output_shapes=*/{PartialTensorShape({})},
          /*node_name=*/kNodeName};
}

// Change just first seed relative to FortyTwo
RandomDatasetParams ChangeSeed() {
  return {/*seed=*/1000,
          /*seed2=*/42,
          /*output_dtypes=*/{DT_INT64},
          /*output_shapes=*/{PartialTensorShape({})},
          /*node_name=*/kNodeName};
}

// Change just second seed relative to FortyTwo
RandomDatasetParams ChangeSeed2() {
  return {/*seed=*/42,
          /*seed2=*/1000,
          /*output_dtypes=*/{DT_INT64},
          /*output_shapes=*/{PartialTensorShape({})},
          /*node_name=*/kNodeName};
}

class ParameterizedGetNextTest : public RandomDatasetOpTest,
                                 public ::testing::WithParamInterface<
                                     GetNextTestCase<RandomDatasetParams>> {};

std::vector<GetNextTestCase<RandomDatasetParams>> GetNextTestCases() {
  return {{/*dataset_params=*/FortyTwo(),
           /*expected_outputs=*/GenerateExpectedData(42, 42, kCount)},
          {/*dataset_params=*/ChangeSeed(),
           /*expected_outputs=*/GenerateExpectedData(1000, 42, kCount)},
          {/*dataset_params=*/ChangeSeed2(),
           /*expected_outputs=*/GenerateExpectedData(42, 1000, kCount)}};
}

TEST_P(ParameterizedGetNextTest, GetNext) {
  auto test_case = GetParam();
  TF_ASSERT_OK(Initialize(&test_case.dataset_params));

  // Can't use DatasetOpsTestBase::CheckIteratorGetNext because the kernel
  // under test produces unbounded input.
  bool end_of_sequence = false;
  std::vector<Tensor> out_tensors;
  while (out_tensors.size() < test_case.expected_outputs.size()) {
    std::vector<Tensor> next;
    TF_ASSERT_OK(
        iterator_->GetNext(iterator_ctx_.get(), &next, &end_of_sequence));

    ASSERT_FALSE(end_of_sequence);  // Dataset should never stop

    out_tensors.insert(out_tensors.end(), next.begin(), next.end());
  }

  TF_ASSERT_OK(ExpectEqual(out_tensors, test_case.expected_outputs,
                           /*compare_order=*/true));
}

INSTANTIATE_TEST_SUITE_P(
    RandomDatasetOpTest, ParameterizedGetNextTest,
    ::testing::ValuesIn(
        std::vector<GetNextTestCase<RandomDatasetParams>>(GetNextTestCases())));

TEST_F(RandomDatasetOpTest, DatasetNodeName) {
  auto dataset_params = FortyTwo();
  TF_ASSERT_OK(Initialize(&dataset_params));
  TF_ASSERT_OK(CheckDatasetNodeName(dataset_params.node_name));
}

TEST_F(RandomDatasetOpTest, DatasetTypeString) {
  auto dataset_params = FortyTwo();
  TF_ASSERT_OK(Initialize(&dataset_params));
  TF_ASSERT_OK(CheckDatasetTypeString(
      name_utils::OpName(RandomDatasetOp::kDatasetType)));
}

TEST_F(RandomDatasetOpTest, DatasetOutputDtypes) {
  auto dataset_params = FortyTwo();
  TF_ASSERT_OK(Initialize(&dataset_params));
  TF_ASSERT_OK(CheckDatasetOutputDtypes({DT_INT64}));
}

TEST_F(RandomDatasetOpTest, DatasetOutputShapes) {
  auto dataset_params = FortyTwo();
  TF_ASSERT_OK(Initialize(&dataset_params));
  TF_ASSERT_OK(CheckDatasetOutputShapes({PartialTensorShape({})}));
}

TEST_F(RandomDatasetOpTest, Cardinality) {
  auto dataset_params = FortyTwo();
  TF_ASSERT_OK(Initialize(&dataset_params));
  TF_ASSERT_OK(CheckDatasetCardinality(kInfiniteCardinality));
}

TEST_F(RandomDatasetOpTest, IteratorOutputDtypes) {
  auto dataset_params = FortyTwo();
  TF_ASSERT_OK(Initialize(&dataset_params));
  TF_ASSERT_OK(CheckIteratorOutputDtypes({DT_INT64}));
}

TEST_F(RandomDatasetOpTest, IteratorOutputShapes) {
  auto dataset_params = FortyTwo();
  TF_ASSERT_OK(Initialize(&dataset_params));
  TF_ASSERT_OK(CheckIteratorOutputShapes({PartialTensorShape({})}));
}

TEST_F(RandomDatasetOpTest, IteratorOutputPrefix) {
  auto dataset_params = FortyTwo();
  TF_ASSERT_OK(Initialize(&dataset_params));
  TF_ASSERT_OK(CheckIteratorPrefix(name_utils::IteratorPrefix(
      RandomDatasetOp::kDatasetType, kIteratorPrefix)));
}

TEST_F(RandomDatasetOpTest, IteratorSaveAndRestore) {
  auto dataset_params = FortyTwo();
  TF_ASSERT_OK(Initialize(&dataset_params));
  TF_ASSERT_OK(CheckIteratorSaveAndRestore(
      kIteratorPrefix,
      /*expected_outputs=*/GenerateExpectedData(42, 42, kCount),
      /*breakpoints=*/{2, 5, 8}));
}

}  // namespace
}  // namespace experimental
}  // namespace data
}  // namespace tensorflow
