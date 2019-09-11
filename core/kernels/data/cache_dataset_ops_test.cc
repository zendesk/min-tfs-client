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
#include "tensorflow/core/kernels/data/cache_dataset_ops.h"

#include "tensorflow/core/kernels/data/dataset_test_base.h"

namespace tensorflow {
namespace data {
namespace {

constexpr char kNodeName[] = "cache_dataset";
constexpr char kIteratorPrefix[] = "Iterator";
constexpr char kFileDatasetPrefix[] = "File";
constexpr char kMemoryDatasetPrefix[] = "Memory";

class CacheDatasetParams : public DatasetParams {
 public:
  template <typename T>
  CacheDatasetParams(T input_dataset_params, string filename,
                     DataTypeVector output_dtypes,
                     std::vector<PartialTensorShape> output_shapes,
                     string node_name)
      : DatasetParams(std::move(output_dtypes), std::move(output_shapes),
                      std::move(node_name), DatasetParamsType::Cache),
        filename_(CreateTensor<tstring>(TensorShape({}), {filename})) {
    auto input_dataset_params_ptr =
        std::make_shared<T>(std::move(input_dataset_params));
    input_dataset_params_group_.emplace_back(
        std::make_pair(std::move(input_dataset_params_ptr), Tensor()));
  }

  Status GetInputs(gtl::InlinedVector<TensorValue, 4>* inputs) override {
    inputs->reserve(input_dataset_params_group_.size());
    for (auto& pair : input_dataset_params_group_) {
      if (!IsDatasetTensor(pair.second)) {
        inputs->clear();
        return errors::Internal(
            "The input dataset is not populated as the dataset tensor yet.");
      } else {
        inputs->emplace_back(TensorValue(&pair.second));
      }
    }
    inputs->emplace_back(TensorValue(&filename_));
    return Status::OK();
  }

  Status GetInputPlaceholder(
      std::vector<string>* input_placeholder) const override {
    *input_placeholder = {CacheDatasetOp::kInputDataset,
                          CacheDatasetOp::kFileName};
    return Status::OK();
  }

  Status GetAttributes(AttributeVector* attr_vector) const override {
    *attr_vector = {{CacheDatasetOp::kOutputTypes, output_dtypes_},
                    {CacheDatasetOp::kOutputShapes, output_shapes_}};
    return Status::OK();
  }

 private:
  Tensor filename_;
};

class CacheDatasetOpTest : public DatasetOpsTestBaseV2 {
 public:
  Status Initialize(DatasetParams& dataset_params) override {
    TF_RETURN_IF_ERROR(DatasetOpsTestBaseV2::Initialize(dataset_params));
    gtl::InlinedVector<TensorValue, 4> inputs;
    TF_RETURN_IF_ERROR(dataset_params.GetInputs(&inputs));
    cache_filename_ = inputs[1].tensor->scalar<tstring>()();
    return Status::OK();
  }

  ~CacheDatasetOpTest() override {
    if (!cache_filename_.empty()) {
      std::vector<string> cache_files;
      Status s = device_->env()->GetMatchingPaths(
          strings::StrCat(cache_filename_, "*"), &cache_files);
      if (!s.ok()) {
        LOG(WARNING) << "Failed to get matching files on " << cache_filename_
                     << "* : " << s.ToString();
      }
      for (const string& path : cache_files) {
        s = device_->env()->DeleteFile(path);
        if (!s.ok()) {
          LOG(WARNING) << "Failed to delete " << path << " : " << s.ToString();
        }
      }
    }
  }

 protected:
  tstring cache_filename_;
};

// Test case 1: cache data in file.
CacheDatasetParams CacheDatasetParams1() {
  auto tensor_slice_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int64>(TensorShape{3, 3, 1},
                                          {0, 1, 2, 3, 4, 5, 6, 7, 8})},
      /*node_name=*/"tensor_slice");
  return CacheDatasetParams(
      std::move(tensor_slice_dataset_params),
      /*filename=*/absl::StrCat(testing::TmpDir(), "/cache_data"),
      /*output_dtypes=*/{DT_INT64},
      /*output_shapes=*/{PartialTensorShape({3, 1})}, kNodeName);
}

// Test case 2: cache empty data in file.
CacheDatasetParams CacheDatasetParams2() {
  auto tensor_slice_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int64>(TensorShape{0}, {})},
      /*node_name=*/"tensor_slice");
  return CacheDatasetParams(
      std::move(tensor_slice_dataset_params),
      /*filename=*/absl::StrCat(testing::TmpDir(), "/cache_data"),
      /*output_dtypes=*/{DT_INT64},
      /*output_shapes=*/{PartialTensorShape({})}, kNodeName);
}

// Test case 3: cache data in memory.
CacheDatasetParams CacheDatasetParams3() {
  auto tensor_slice_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int64>(TensorShape{3, 3, 1},
                                          {0, 1, 2, 3, 4, 5, 6, 7, 8})},
      /*node_name=*/"tensor_slice");
  return CacheDatasetParams(std::move(tensor_slice_dataset_params),
                            /*filename=*/"",
                            /*output_dtypes=*/{DT_INT64},
                            /*output_shapes=*/{PartialTensorShape({3, 1})},
                            kNodeName);
}

// Test case 4: cache empty data in memory.
CacheDatasetParams CacheDatasetParams4() {
  auto tensor_slice_dataset_params = TensorSliceDatasetParams(
      /*components=*/{CreateTensor<int64>(TensorShape{0}, {})},
      /*node_name=*/"tensor_slice");
  return CacheDatasetParams(std::move(tensor_slice_dataset_params),
                            /*filename=*/"",
                            /*output_dtypes=*/{DT_INT64},
                            /*output_shapes=*/{PartialTensorShape({})},
                            kNodeName);
}

std::vector<GetNextTestCase<CacheDatasetParams>> GetNextTestCases() {
  return {{/*dataset_params=*/CacheDatasetParams1(),
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape({3, 1}),
                                {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}})},
          {/*dataset_params=*/CacheDatasetParams2(),
           /*expected_outputs=*/{}},
          {/*dataset_params=*/CacheDatasetParams3(),
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape({3, 1}),
                                {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}})},
          {/*dataset_params=*/CacheDatasetParams4(),
           /*expected_outputs=*/{}}};
}

class ParameterizedGetNextTest : public CacheDatasetOpTest,
                                 public ::testing::WithParamInterface<
                                     GetNextTestCase<CacheDatasetParams>> {};

TEST_P(ParameterizedGetNextTest, GetNext) {
  auto test_case = GetParam();
  TF_ASSERT_OK(Initialize(test_case.dataset_params));

  // Test the write mode.
  bool end_of_sequence = false;
  std::vector<Tensor> out_tensors;
  while (!end_of_sequence) {
    std::vector<Tensor> next;
    TF_EXPECT_OK(
        iterator_->GetNext(iterator_ctx_.get(), &next, &end_of_sequence));
    out_tensors.insert(out_tensors.end(), next.begin(), next.end());
  }
  TF_EXPECT_OK(ExpectEqual(out_tensors, test_case.expected_outputs,
                           /*compare_order*/ true));

  // Test the read mode.
  TF_ASSERT_OK(
      dataset_->MakeIterator(iterator_ctx_.get(), kIteratorPrefix, &iterator_));
  end_of_sequence = false;
  out_tensors.clear();
  while (!end_of_sequence) {
    std::vector<Tensor> next;
    TF_EXPECT_OK(
        iterator_->GetNext(iterator_ctx_.get(), &next, &end_of_sequence));
    out_tensors.insert(out_tensors.end(), next.begin(), next.end());
  }
  TF_EXPECT_OK(ExpectEqual(out_tensors, test_case.expected_outputs,
                           /*compare_order*/ true));
}

INSTANTIATE_TEST_SUITE_P(CacheDatasetOpTest, ParameterizedGetNextTest,
                         ::testing::ValuesIn(GetNextTestCases()));

TEST_F(CacheDatasetOpTest, DatasetNodeName) {
  auto dataset_params = CacheDatasetParams1();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckDatasetNodeName(dataset_params.node_name()));
}

TEST_F(CacheDatasetOpTest, DatasetTypeString) {
  auto dataset_params = CacheDatasetParams1();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(
      CheckDatasetTypeString(name_utils::OpName(CacheDatasetOp::kDatasetType)));
}

TEST_F(CacheDatasetOpTest, DatasetOutputDtypes) {
  auto dataset_params = CacheDatasetParams1();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckDatasetOutputDtypes({DT_INT64}));
}

std::vector<DatasetOutputShapesTestCase<CacheDatasetParams>>
DatasetOutputShapesTestCases() {
  return {{/*dataset_params=*/CacheDatasetParams1(),
           /*expected_output_shapes=*/{PartialTensorShape({3, 1})}},
          {/*dataset_params=*/CacheDatasetParams2(),
           /*expected_output_shapes=*/{PartialTensorShape({})}},
          {/*dataset_params=*/CacheDatasetParams3(),
           /*expected_output_shapes=*/
           /*expected_output_shapes=*/{PartialTensorShape({3, 1})}},
          {/*dataset_params=*/CacheDatasetParams4(),
           /*expected_output_shapes=*/{PartialTensorShape({})}}};
}

DATASET_OUTPUT_SHAPES_TEST_P(CacheDatasetOpTest, CacheDatasetParams,
                             DatasetOutputShapesTestCases())

std::vector<CardinalityTestCase<CacheDatasetParams>> CardinalityTestCases() {
  return {{/*dataset_params=*/CacheDatasetParams1(),
           /*expected_cardinality=*/3},
          {/*dataset_params=*/CacheDatasetParams2(),
           /*expected_cardinality=*/0},
          {/*dataset_params=*/CacheDatasetParams3(),
           /*expected_cardinality=*/3},
          {/*dataset_params=*/CacheDatasetParams4(),
           /*expected_cardinality=*/0}};
}

DATASET_CARDINALITY_TEST_P(CacheDatasetOpTest, CacheDatasetParams,
                           CardinalityTestCases())

TEST_F(CacheDatasetOpTest, IteratorOutputDtypes) {
  auto dataset_params = CacheDatasetParams1();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckIteratorOutputDtypes({DT_INT64}));
}

std::vector<IteratorOutputShapesTestCase<CacheDatasetParams>>
IteratorOutputShapesTestCases() {
  return {{/*dataset_params=*/CacheDatasetParams1(),
           /*expected_output_shapes=*/{PartialTensorShape({3, 1})}},
          {/*dataset_params=*/CacheDatasetParams2(),
           /*expected_output_shapes=*/{PartialTensorShape({})}},
          {/*dataset_params=*/CacheDatasetParams3(),
           /*expected_output_shapes=*/
           /*expected_output_shapes=*/{PartialTensorShape({3, 1})}},
          {/*dataset_params=*/CacheDatasetParams4(),
           /*expected_output_shapes=*/{PartialTensorShape({})}}};
}

ITERATOR_OUTPUT_SHAPES_TEST_P(CacheDatasetOpTest, CacheDatasetParams,
                              IteratorOutputShapesTestCases())

TEST_F(CacheDatasetOpTest, IteratorPrefix) {
  auto dataset_params = CacheDatasetParams1();
  TF_ASSERT_OK(Initialize(dataset_params));
  name_utils::IteratorPrefixParams iterator_prefix_params;
  iterator_prefix_params.dataset_prefix =
      cache_filename_.empty() ? kMemoryDatasetPrefix : kFileDatasetPrefix;
  TF_ASSERT_OK(CheckIteratorPrefix(name_utils::IteratorPrefix(
      CacheDatasetOp::kDatasetType, dataset_params.iterator_prefix(),
      iterator_prefix_params)));
}

std::vector<IteratorSaveAndRestoreTestCase<CacheDatasetParams>>
IteratorSaveAndRestoreTestCases() {
  return {{/*dataset_params=*/CacheDatasetParams1(),
           /*breakpoints*/ {0, 2, 4, 11},
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape({3, 1}),
                                {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}})},
          {/*dataset_params=*/CacheDatasetParams2(),
           /*breakpoints*/ {0, 2, 4, 11},
           /*expected_outputs=*/{}},
          {/*dataset_params=*/CacheDatasetParams3(),
           /*breakpoints*/ {0, 2, 4, 11},
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape({3, 1}),
                                {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}})},
          {/*dataset_params=*/CacheDatasetParams4(),
           /*breakpoints*/ {0, 2, 4, 11},
           /*expected_outputs=*/{}}};
}

class ParameterizedIteratorSaveAndRestoreTest
    : public CacheDatasetOpTest,
      public ::testing::WithParamInterface<
          IteratorSaveAndRestoreTestCase<CacheDatasetParams>> {};

TEST_P(ParameterizedIteratorSaveAndRestoreTest, Roundtrip) {
  auto test_case = GetParam();
  TF_ASSERT_OK(Initialize(test_case.dataset_params));

  bool end_of_sequence = false;
  std::vector<Tensor> out_tensors;
  // For MemoryIterator in the read mode, the cache needs to be completed
  // before it has been read.
  if (cache_filename_.empty()) {
    while (!end_of_sequence) {
      TF_EXPECT_OK(iterator_->GetNext(iterator_ctx_.get(), &out_tensors,
                                      &end_of_sequence));
    }
    end_of_sequence = false;
    out_tensors.clear();
    TF_ASSERT_OK(dataset_->MakeIterator(iterator_ctx_.get(), kIteratorPrefix,
                                        &iterator_));
  }

  std::unique_ptr<SerializationContext> serialization_ctx;
  TF_ASSERT_OK(CreateSerializationContext(&serialization_ctx));
  int cur_iteration = 0;
  auto expected_outputs_it = test_case.expected_outputs.begin();
  for (int breakpoint : test_case.breakpoints) {
    VariantTensorData data;
    VariantTensorDataWriter writer(&data);
    TF_EXPECT_OK(iterator_->Save(serialization_ctx.get(), &writer));
    TF_EXPECT_OK(writer.Flush());
    VariantTensorDataReader reader(&data);
    TF_EXPECT_OK(RestoreIterator(iterator_ctx_.get(), &reader, kIteratorPrefix,
                                 *dataset_, &iterator_));

    while (cur_iteration <= breakpoint) {
      out_tensors.clear();
      TF_EXPECT_OK(iterator_->GetNext(iterator_ctx_.get(), &out_tensors,
                                      &end_of_sequence));
      if (!end_of_sequence) {
        EXPECT_LT(expected_outputs_it, test_case.expected_outputs.end());
        TF_EXPECT_OK(ExpectEqual(out_tensors.back(), *expected_outputs_it));
        expected_outputs_it++;
      }
      cur_iteration++;
    }

    if (breakpoint >= dataset_->Cardinality()) {
      EXPECT_TRUE(end_of_sequence);
      EXPECT_EQ(expected_outputs_it, test_case.expected_outputs.end());
    } else {
      EXPECT_FALSE(end_of_sequence);
    }
  }
}

INSTANTIATE_TEST_CASE_P(CacheDatasetOpTest,
                        ParameterizedIteratorSaveAndRestoreTest,
                        ::testing::ValuesIn(IteratorSaveAndRestoreTestCases()));

}  // namespace
}  // namespace data
}  // namespace tensorflow
