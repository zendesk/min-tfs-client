/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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

// This file contains utilities to make it easier to invoke functions generated
// by tfcompile.  Usage of these utilities is optional.

#ifndef TENSORFLOW_COMPILER_AOT_RUNTIME_H_
#define TENSORFLOW_COMPILER_AOT_RUNTIME_H_

#include "tensorflow/core/platform/types.h"

namespace tensorflow {
namespace tfcompile {
namespace runtime {

// Align to 64-bytes, to mimic tensorflow::Allocator::kAllocatorAlignment.
static constexpr size_t kAlign = 64;

// aligned_buffer_bytes returns the sum of each size in `sizes`, skipping -1
// values.  There are `n` entries in `sizes`.  Each buffer is aligned to kAlign
// byte boundaries.
size_t aligned_buffer_bytes(const intptr_t* sizes, size_t n);

// MallocContiguousBuffers allocates buffers for use by the entry point
// generated by tfcompile.  `sizes` is an array of byte sizes for each buffer,
// where -1 causes the buffer pointer to be nullptr.  There are `n` entries in
// `sizes`.  If `annotate_initialized` is set, the allocated memory will be
// annotated as having been initialized - this is useful when allocating
// temporary buffers.
//
// A single contiguous block of memory is allocated, and portions of it are
// parceled out into `bufs`, which must have space for `n` entries.  Returns the
// head of the allocated contiguous block, which should be passed to
// FreeContiguous when the buffers are no longer in use.
void* MallocContiguousBuffers(const intptr_t* sizes, size_t n, void** bufs,
                              bool annotate_initialized);

// FreeContiguous frees the contiguous block of memory allocated by
// MallocContiguousBuffers.
void FreeContiguous(void* contiguous);

}  // namespace runtime
}  // namespace tfcompile
}  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_AOT_RUNTIME_H_
