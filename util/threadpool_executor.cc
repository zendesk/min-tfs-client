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

#include "tensorflow_serving/util/threadpool_executor.h"

namespace tensorflow {
namespace serving {

ThreadPoolExecutor::ThreadPoolExecutor(Env* const env, const string& name,
                                       int num_threads)
    : thread_pool_(env, name, num_threads) {}

ThreadPoolExecutor::~ThreadPoolExecutor() {}

void ThreadPoolExecutor::Schedule(std::function<void()> fn) {
  thread_pool_.Schedule(fn);
}

bool ThreadPoolExecutor::HasPendingClosures() const {
  return thread_pool_.HasPendingClosures();
}

}  // namespace serving
}  // namespace tensorflow
