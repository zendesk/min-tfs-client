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

#ifndef TENSORFLOW_SERVING_SOURCES_STORAGE_PATH_STATIC_STORAGE_PATH_SOURCE_H_
#define TENSORFLOW_SERVING_SOURCES_STORAGE_PATH_STATIC_STORAGE_PATH_SOURCE_H_

#include <memory>

#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/platform/macros.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow_serving/core/source.h"
#include "tensorflow_serving/core/storage_path.h"
#include "tensorflow_serving/sources/storage_path/static_storage_path_source.pb.h"

namespace tensorflow {
namespace serving {

// A StoragePathSource that calls the aspired-versions callback exactly once,
// with a single hard-coded servable and version path.
// Useful for testing and experimental deployments.
class StaticStoragePathSource : public Source<StoragePath> {
 public:
  static Status Create(const StaticStoragePathSourceConfig& config,
                       std::unique_ptr<StaticStoragePathSource>* result);
  ~StaticStoragePathSource() override = default;

  void SetAspiredVersionsCallback(AspiredVersionsCallback callback) override;

 private:
  StaticStoragePathSource() = default;

  StaticStoragePathSourceConfig config_;

  TF_DISALLOW_COPY_AND_ASSIGN(StaticStoragePathSource);
};

}  // namespace serving
}  // namespace tensorflow

#endif  // TENSORFLOW_SERVING_SOURCES_STORAGE_PATH_STATIC_STORAGE_PATH_SOURCE_H_
