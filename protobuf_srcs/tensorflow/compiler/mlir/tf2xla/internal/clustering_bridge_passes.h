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

#ifndef TENSORFLOW_COMPILER_MLIR_TF2XLA_INTERNAL_CLUSTERING_BRIDGE_PASSES_H_
#define TENSORFLOW_COMPILER_MLIR_TF2XLA_INTERNAL_CLUSTERING_BRIDGE_PASSES_H_

#include "llvm/ADT/StringRef.h"
#include "mlir/Pass/PassManager.h"  // from @llvm-project

namespace tensorflow {
namespace tf2xla {
namespace internal {

// Given the pass manager, add Bridge passes to cluster the input.
void AddBridgeClusteringPipelinePasses(
    mlir::OpPassManager& pm, llvm::StringRef module_name = llvm::StringRef());

};  // namespace internal
};  // namespace tf2xla
};  // namespace tensorflow

#endif  // TENSORFLOW_COMPILER_MLIR_TF2XLA_INTERNAL_CLUSTERING_BRIDGE_PASSES_H_
