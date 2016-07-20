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

#include "tensorflow_serving/core/test_util/fake_storage_path_source_adapter.h"

#include "tensorflow/core/lib/core/errors.h"

namespace tensorflow {
namespace serving {
namespace test_util {

FakeStoragePathSourceAdapter::FakeStoragePathSourceAdapter(
    const string& suffix, std::function<void(const string&)> call_on_destruct)
    : suffix_(suffix), call_on_destruct_(call_on_destruct) {}

FakeStoragePathSourceAdapter::~FakeStoragePathSourceAdapter() {
  if (call_on_destruct_) {
    call_on_destruct_(suffix_);
  }
}

Status FakeStoragePathSourceAdapter::Convert(
    const StoragePath& data, StoragePath* const converted_data) {
  if (data == "invalid") {
    return errors::InvalidArgument(
        "FakeStoragePathSourceAdapter Convert() dutifully failing on "
        "\"invalid\" "
        "data");
  }
  *converted_data =
      suffix_.empty() ? data : strings::StrCat(data, "/", suffix_);
  return Status::OK();
}

}  // namespace test_util
}  // namespace serving
}  // namespace tensorflow
