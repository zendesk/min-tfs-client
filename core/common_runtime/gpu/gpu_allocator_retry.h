/* Copyright 2015 Google Inc. All Rights Reserved.

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

#ifndef TENSORFLOW_CORE_COMMON_RUNTIME_GPU_GPU_ALLOCATOR_RETRY_H_
#define TENSORFLOW_CORE_COMMON_RUNTIME_GPU_GPU_ALLOCATOR_RETRY_H_

#include "tensorflow/core/platform/port.h"
#include "tensorflow/core/public/env.h"

namespace tensorflow {

// A retrying wrapper for a memory allocator.
class GPUAllocatorRetry {
 public:
  GPUAllocatorRetry();

  // Call 'alloc_func' to obtain memory.  On first call,
  // 'verbose_failure' will be false.  If return value is nullptr,
  // then wait up to 'max_millis_to_wait' milliseconds, retrying each
  // time a call to DeallocateRaw() is detected, until either a good
  // pointer is returned or the deadline is exhausted.  If the
  // deadline is exahusted, try one more time with 'verbose_failure'
  // set to true.  The value returned is either the first good pointer
  // obtained from 'alloc_func' or nullptr.
  void* AllocateRaw(std::function<void*(size_t alignment, size_t num_bytes,
                                        bool verbose_failure)> alloc_func,
                    int max_millis_to_wait, size_t alignment, size_t bytes);

  // Calls dealloc_func(ptr) and then notifies any threads blocked in
  // AllocateRaw() that would like to retry.
  void DeallocateRaw(std::function<void(void* ptr)> dealloc_func, void* ptr);

 private:
  Env* env_;
  mutex mu_;
  condition_variable memory_returned_;
};
}  // namespace tensorflow
#endif  // TENSORFLOW_CORE_COMMON_RUNTIME_GPU_GPU_ALLOCATOR_RETRY_H_
