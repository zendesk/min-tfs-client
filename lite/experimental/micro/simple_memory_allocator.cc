/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/lite/experimental/micro/simple_memory_allocator.h"

#include "tensorflow/lite/core/api/flatbuffer_conversions.h"
#include "tensorflow/lite/experimental/micro/memory_helpers.h"

namespace tflite {

uint8_t* SimpleMemoryAllocator::AllocateFromTail(size_t size,
                                                 size_t alignment) {
  uint8_t* previous_free = (data_ + data_size_max_) - data_size_;
  uint8_t* current_data = previous_free - size;
  uint8_t* aligned_result = AlignPointerDown(current_data, alignment);
  size_t aligned_size = (previous_free - aligned_result);
  if ((data_size_ + aligned_size) > data_size_max_) {
    // TODO(petewarden): Add error reporting beyond returning null!
    return nullptr;
  }
  data_size_ += aligned_size;
  return aligned_result;
}

}  // namespace tflite
