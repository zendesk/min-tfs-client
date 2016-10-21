/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/core/framework/lookup_interface.h"

#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/lib/core/errors.h"

namespace tensorflow {
namespace lookup {

namespace {
Status CheckKeyShape(const TensorShape& table_key_shape,
                     const TensorShape& key_shape) {
  if (!TensorShapeUtils::EndsWith(key_shape, table_key_shape)) {
    return errors::InvalidArgument("Input key shape ", key_shape.DebugString(),
                                   " must end with the table's key shape ",
                                   table_key_shape.DebugString());
  }
  return Status::OK();
}
}  // namespace

Status LookupInterface::CheckKeyAndValueTypes(const Tensor& keys,
                                              const Tensor& values) {
  if (keys.dtype() != key_dtype()) {
    return errors::InvalidArgument("Key must be type ", key_dtype(),
                                   " but got ", keys.dtype());
  }
  if (values.dtype() != value_dtype()) {
    return errors::InvalidArgument("Value must be type ", value_dtype(),
                                   " but got ", values.dtype());
  }
  return Status::OK();
}

Status LookupInterface::CheckKeyAndValueTensors(const Tensor& key,
                                                const Tensor& value) {
  TF_RETURN_IF_ERROR(CheckKeyAndValueTypes(key, value));
  TF_RETURN_IF_ERROR(CheckKeyShape(key_shape(), key.shape()));

  TensorShape expected_value_shape = key.shape();
  for (int i = 0; i < key_shape().dims(); ++i) {
    expected_value_shape.RemoveDim(expected_value_shape.dims() - 1);
  }
  expected_value_shape.AppendShape(value_shape());
  if (value.shape() != expected_value_shape) {
    return errors::InvalidArgument(
        "Expected shape ", expected_value_shape.DebugString(),
        " for value, got ", value.shape().DebugString());
  }
  return Status::OK();
}

Status LookupInterface::CheckFindArguments(const Tensor& key,
                                           const Tensor& default_value) {
  TF_RETURN_IF_ERROR(CheckKeyAndValueTypes(key, default_value));
  TF_RETURN_IF_ERROR(CheckKeyShape(key_shape(), key.shape()));
  if (default_value.shape() != value_shape()) {
    return errors::InvalidArgument(
        "Expected shape ", value_shape().DebugString(),
        " for default value, got ", default_value.shape().DebugString());
  }
  return Status::OK();
}

}  // namespace lookup
}  // namespace tensorflow
