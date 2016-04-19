/* Copyright 2016 Google Inc. All Rights Reserved.

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

#include "tensorflow/core/framework/allocator.h"
#include "tensorflow/core/framework/fake_input.h"
#include "tensorflow/core/framework/node_def_builder.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_testutil.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/kernels/ops_testutil.h"
#include "tensorflow/core/platform/test.h"

namespace tensorflow {

namespace {

class SparseReduceSumOpTest : public OpsTestBase {
 protected:
  template <typename T>
  void MakeOp() {
    DataType value_type = tensorflow::DataTypeToEnum<T>::value;
    TF_ASSERT_OK(NodeDefBuilder("sparse_reduce_sum", "SparseReduceSum")
                     .Input(FakeInput(DT_INT64))
                     .Input(FakeInput(value_type))
                     .Input(FakeInput(DT_INT64))
                     .Input(FakeInput(DT_INT32))
                     .Attr("T", value_type)
                     .Finalize(node_def()));
    TF_ASSERT_OK(InitOp());
  }
};

TEST_F(SparseReduceSumOpTest, SimpleReduce) {
  MakeOp<float>();

  // [    1]
  // [2    ]
  // [3   4]

  const auto indices_shape = TensorShape({4, 2});
  const gtl::ArraySlice<int64> indices = {0, 1, 1, 0, 2, 0, 2, 1};
  const gtl::ArraySlice<int64> shape = {3, 2};

  AddInputFromArray<int64>(indices_shape, indices);
  AddInputFromArray<float>(TensorShape({4}), {1, 2, 3, 4});
  AddInputFromArray<int64>(TensorShape({2}), shape);
  AddInputFromArray<int32>(TensorShape({1}), {0});  // reduction axes

  TF_ASSERT_OK(RunOpKernel());

  Tensor expected_by_0(allocator(), DT_FLOAT, TensorShape({2}));
  test::FillValues<float>(&expected_by_0, {5, 5});
  test::ExpectTensorEqual<float>(expected_by_0, *GetOutput(0));
}

}  // namespace

}  // namespace tensorflow
