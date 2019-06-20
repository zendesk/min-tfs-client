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
#ifndef TENSORFLOW_CORE_KERNELS_DATA_NAME_UTILS_H_
#define TENSORFLOW_CORE_KERNELS_DATA_NAME_UTILS_H_

#include <vector>

#include "tensorflow/core/lib/strings/strcat.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {
namespace data {
namespace name_utils {

extern const char kDelimiter[];
extern const char kDefaultDatasetDebugStringPrefix[];

struct OpNameParams {
  OpNameParams() = default;

  explicit OpNameParams(int op_version) : op_version(op_version){};

  int op_version = 1;
};

struct DatasetDebugStringParams {
  DatasetDebugStringParams() = default;

  template <typename... T>
  explicit DatasetDebugStringParams(int op_version, string dataset_prefix,
                                    const T&... args)
      : op_version(op_version),
        dataset_prefix(std::move(dataset_prefix)),
        args({static_cast<const strings::AlphaNum&>(args).data()...}){};

  int op_version = 1;
  string dataset_prefix = "";
  std::vector<string> args;
};

// Merge the given args in the format of "(arg1, arg2, ..., argn)".
//
// e.g. ArgsToString({"1", "2", "3"}) -> "(1, 2, 3)"; ArgsToString({}) -> "".
string ArgsToString(const std::vector<string>& args);

// Returns the dataset op name.
//
// e.g. OpName("Map") -> "MapDataset".
string OpName(const string& dataset_type);

// Returns the dataset op names.
//
// e.g. OpName(ConcatenateDatasetOp::kDatasetType, OpNameParams())
// -> "ConcatenateDataset"
//
// OpName(ParallelInterleaveDatasetOp::kDatasetType, OpNameParams(2)),
// -> "ParallelInterleaveDatasetV2"
string OpName(const string& dataset_type, const OpNameParams& params);

// Returns a human-readable debug string for this dataset in the format of
// "FooDatasetOp(arg1, arg2, ...)::Dataset".
//
// e.g. DatasetDebugString("Map") -> "MapDatasetOp::Dataset";
string DatasetDebugString(const string& dataset_type);

// Returns a human-readable debug string for this dataset in the format of
// "FooDatasetOp(arg1, arg2, ...)::Dataset".
//
// e.g. DatasetDebugString(
// "Shuffle", DatasetDebugStringParams(1, "FixedSeed", 10, 1, 2))
// -> "ShuffleDatasetOp(10, 1, 2)::FixedSeedDataset";
//
// DatasetDebugString("ParallelInterleave", DatasetDebugStringParams(2, ""))
// -> "ParallelInterleaveDatasetV2Op::Dataset").
string DatasetDebugString(const string& dataset_type,
                          const DatasetDebugStringParams& params);

// Returns a string that identifies the sequence of iterators leading up to
// the iterator of this dataset.
//
// e.g. IteratorPrefix("Map", "Iterator::range") -> "Iterator::Range::Map".
string IteratorPrefix(const string& dataset_type, const string& prefix);

}  // namespace name_utils
}  // namespace data
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_KERNELS_DATA_NAME_UTILS_H_
