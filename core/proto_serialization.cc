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
#include "tensorflow_serving/core/proto_serialization.h"
#include "tensorflow/core/platform/logging.h"

namespace tensorflow {
namespace serving {

bool SerializeToStringDeterministic(const protobuf::MessageLite& msg,
                                    string* result) {
  DCHECK_LE(msg.ByteSizeLong(), static_cast<size_t>(INT_MAX));
  const int size = static_cast<int>(msg.ByteSizeLong());
  *result = string(size, '\0');
  protobuf::io::ArrayOutputStream array_stream(&(*result)[0], size);
  protobuf::io::CodedOutputStream output_stream(&array_stream);
  output_stream.SetSerializationDeterministic(true);
  msg.SerializeWithCachedSizes(&output_stream);
  return !output_stream.HadError() && size == output_stream.ByteCount();
}

}  // namespace serving
}  // namespace tensorflow
