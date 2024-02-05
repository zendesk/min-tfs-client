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

#include "tensorflow/core/tfrt/saved_model/utils/serialize_bef_utils.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "llvm/Support/ToolOutputFile.h"
#include "mlir/Support/FileUtilities.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/utils/dump_mlir_util.h"
#include "tensorflow/core/platform/status.h"
#include "tsl/platform/env.h"
#include "tfrt/bef/bef_buffer.h"  // from @tf_runtime

namespace tensorflow {
namespace tfrt_stub {

absl::Status SerializeBEF(const tfrt::BefBuffer &bef,
                          const std::string &filepath) {
  std::string errorMessage;
  auto output = mlir::openOutputFile(filepath, &errorMessage);
  (output->os()).write(reinterpret_cast<const char *>(bef.data()), bef.size());
  output->keep();
  LOG(INFO) << "Completed serializing BEF to: " << filepath;

  return absl::OkStatus();
}

absl::StatusOr<tfrt::BefBuffer> DeserializeBEFBuffer(
    const std::string &filepath) {
  std::string data;
  TF_CHECK_OK(ReadFileToString(tsl::Env::Default(), filepath, &data));
  tfrt::BefBuffer bef(data.begin(), data.end());
  LOG(INFO) << "Successfully loaded serialized BEF from: " << filepath;
  return bef;
}

}  // namespace tfrt_stub
}  // namespace tensorflow
