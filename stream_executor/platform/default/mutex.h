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

#ifndef TENSORFLOW_STREAM_EXECUTOR_PLATFORM_DEFAULT_MUTEX_H_
#define TENSORFLOW_STREAM_EXECUTOR_PLATFORM_DEFAULT_MUTEX_H_

#include "tensorflow/stream_executor/platform/mutex.h"

namespace perftools {
namespace gputools {

#undef mutex_lock
#undef tf_shared_lock

using tensorflow::ConditionResult;
using tensorflow::WaitForMilliseconds;
using tensorflow::condition_variable;
using tensorflow::mutex;
using tensorflow::mutex_lock;
using tensorflow::tf_shared_lock;

#define mutex_lock(x) static_assert(0, "mutex_lock_decl_missing_var_name");
#define tf_shared_lock(x) \
  static_assert(0, "tf_shared_lock_decl_missing_var_name");

/*
#ifdef STREAM_EXECUTOR_USE_SHARED_MUTEX
// TODO(vrv): Annotate these with ACQUIRE_SHARED after implementing
// as classes.
typedef std::shared_lock<BaseMutex> shared_lock;
//typedef std::condition_variable_any condition_variable;
#else
typedef mutex_lock shared_lock;
//typedef std::condition_variable condition_variable;
#endif

 inline ConditionResult WaitForMilliseconds(shared_lock* mu,
                                            ConditionVariableForMutex* cv, int64 ms) {
  std::cv_status s = cv->wait_for(*mu, std::chrono::milliseconds(ms));
  return (s == std::cv_status::timeout) ? kCond_Timeout : kCond_MaybeNotified;
}
*/
}  // namespace gputools
}  // namespace perftools

#endif  // TENSORFLOW_STREAM_EXECUTOR_PLATFORM_DEFAULT_MUTEX_H_
