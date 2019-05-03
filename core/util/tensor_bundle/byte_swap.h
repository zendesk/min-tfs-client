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

#ifndef BYTE_SWAP_H
#define BYTE_SWAP_H

#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/framework/tensor.h"


// Define basic byte swapping operations.
// These operations must be macros to use compiler intrinsics.
// Uses the glibc macros when available.  See bswap(3) for more info.
// Beyond the use of glibc macros, the code here is written for portability,
// not speed. If byte swapping becomes part of a fast path, then the function
// ByteSwapArray() below should be rewritten to use architecture-
// appropriate SIMD instructions that swap multiple words at once.

#ifdef __linux__
#include <byteswap.h>

#define BYTE_SWAP_16(x) bswap_16 (x)
#define BYTE_SWAP_32(x) bswap_32 (x)
#define BYTE_SWAP_64(x) bswap_64 (x)

#else // ifndef __linux__

// Fall back on a non-optimized implementation on non-Glibc based targets.
// This code swaps one byte at a time and is probably an order of magnitude
// slower. Currently this performance difference is not important, because
// byte swapping only happens when importing a checkpoint from one hardware
// architecture onto a different architecture.

#define BYTE_SWAP_16(x) (  \
  (((x) & 0x00ff) << 8) \
  | (((x) & 0xff00) >> 8) \
)

#define BYTE_SWAP_32(x) (\
  (((x) & 0x000000ffU) << 24) \
  | (((x) & 0x0000ff00U) << 8) \
  | (((x) & 0x00ff0000U) >> 8) \
  | (((x) & 0xff000000U) >> 24) \
)

#define BYTE_SWAP_64(x) (\
    (((x) & 0x00000000000000ffUL) << 56) \
    | (((x) & 0x000000000000ff00UL) << 40) \
    | (((x) & 0x0000000000ff0000UL) << 24) \
    | (((x) & 0x00000000ff000000UL) << 8) \
    | (((x) & 0x000000ff00000000UL) >> 8) \
    | (((x) & 0x0000ff0000000000UL) >> 24) \
    | (((x) & 0x00ff000000000000UL) >> 40) \
    | (((x) & 0xff00000000000000UL) >> 56) \
)

#endif // defined(__linux__)

namespace tensorflow {

// Byte-swap an entire array of atomic C/C++ types in place.
//
// Note: When calling this function on arrays of std::complex<> types,
// multiply the number of elements by 2 and divide the bytes per element by 2.
//
// Args:
//  array: Pointer to the beginning of the array
//  bytes_per_elem: Number of bytes in each element of the array
//  array_len: Number of elements in the array
//
// Returns: Status::OK() on success, -1 otherwise
//
Status ByteSwapArray(char *array, size_t bytes_per_elem, int array_len);

// Byte-swap a tensor's backing buffer in place.
//
// Args:
//  t: Tensor to be modified IN PLACE. Any tensors that share a backing
//     buffer with this one will also end up byte-swapped.
// Returns: Status::OK() on success, -1 otherwise
// TODO(frreiss): Should this be a member of the Tensor class?
Status ByteSwapTensor(Tensor *t);

} // namespace tensorflow

#endif // BYTE_SWAP_H
