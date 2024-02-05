/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#include <functional>
#include <memory>

#include "llvm/ADT/STLExtras.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"  // from @llvm-project
#include "mlir/IR/Attributes.h"  // from @llvm-project
#include "mlir/IR/Block.h"  // from @llvm-project
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/BuiltinTypes.h"  // from @llvm-project
#include "mlir/IR/MLIRContext.h"  // from @llvm-project
#include "mlir/Pass/Pass.h"  // from @llvm-project
#include "mlir/Support/LLVM.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/lite/ir/tfl_ops.h"
#include "tensorflow/compiler/mlir/lite/transforms/passes.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/ir/tf_types.h"

namespace mlir {
namespace TFL {
namespace {
#define GEN_PASS_DEF_PINOPSWITHSIDEEFFECTSPASS
#include "tensorflow/compiler/mlir/lite/transforms/passes.h.inc"

bool IsResourceTensor(Value value) {
  const auto tensor_type = value.getType().dyn_cast<TensorType>();
  return tensor_type &&
         tensor_type.getElementType().isa<mlir::TF::ResourceType>();
}

// The default criterion for operations being considered as causing or being
// dependent on side effects. Reflects the current runtime logic; see below.
bool OpHasSideEffects(Operation *op) {
  // Note that TFL::IfOp are only ever instantiated in flatbuffer_export; until
  // then, they are represented as mlir::TF::IfOp. We add them here, anyway, to
  // be future-proof.
  if (llvm::isa<TF::IfOp, TFL::IfOp, TFL::CallOnceOp, TFL::WhileOp>(op))
    return true;
  for (auto operand : op->getOperands()) {
    if (IsResourceTensor(operand)) return true;
  }
  for (auto result : op->getResults()) {
    if (IsResourceTensor(result)) return true;
  }
  return false;
}

// This transformation pass takes an operation that has or depends on side
// effects and wraps it in a TFL::ControlNodeOp, which is made to depend on the
// control token generated by the most recent preceding such operation, if
// any. This copies the logic that is currently executed at runtime (in
// tensorflow/lite/core/subgraph). That runtime logic will now be a no-op for
// models that were generated with this pass.
//
// For purposes of this pass, an operator is considered to have/depend on side
// effects if
//  - it involves calling a different function
//  - it involves accessing resource variables
//
// Note that these criteria are more restrictive than necessary:
//  - they will force a fixed order on operations that read from/write to
//    *different* variables
//  - they make the blanket assumption that any functions called cause or depend
//    on side effects (i.e., access resource variables.)
//
// By moving the logic to compile time, we will be able to do a finer-grained
// data flow analysis in the future, which will enable more optimizations.
// This could happen in two steps:
// (1) build multiple dependency chains (one per variable), still treating
//     function/subgraph calls as black boxes (i.e., all variables would
//     be assumed to be read and modified within control operations)
// (2) Extend the variable dependency analysis across function boundaries.
class PinOpsWithSideEffectsPass
    : public impl::PinOpsWithSideEffectsPassBase<PinOpsWithSideEffectsPass> {
 public:
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(PinOpsWithSideEffectsPass)

  explicit PinOpsWithSideEffectsPass(const std::function<bool(Operation *)> &
                                         op_has_side_effects = OpHasSideEffects)
      : op_has_side_effects_(op_has_side_effects) {}

  void runOnOperation() override;

 private:
  // This will be used recursively.
  const std::function<bool(Operation *)> op_has_side_effects_;
};

void PinOpsWithSideEffectsPass::runOnOperation() {
  auto fn = getOperation();
  // We're assuming (and checking) that there are no tfl::ControlNodeOps present
  // before this pass. We could relax this requirement by defining folding logic
  // for them.
  if (fn.walk([&](TFL::ControlNodeOp) {
          return WalkResult::interrupt();
        }).wasInterrupted()) {
    fn.emitOpError("Can't have control ops in this pass.");
    signalPassFailure();
  }

  llvm::SmallVector<Operation *, 4> ops_with_side_effects;

  // We're iterating over all operations at the top block level, excluding
  // the return operation (which otherwise would be recognized as being
  // susceptible to side effects when returning a resource variable.)
  // We only need to consider functions with single-block bodies, as
  // this is an assumption flatbuffer_export makes, and this pass is
  // operating on IRs ready for exporting.
  for (Operation &op : fn.getBody().front().without_terminator()) {
    // We have to recurse, since we might have wrapped a side-effectful operator
    // in a tfl::CustomTfOp.
    if (op.walk([&](Operation *inner_op) {
            return op_has_side_effects_(inner_op) ? WalkResult::interrupt()
                                                  : WalkResult::advance();
          }).wasInterrupted()) {
      ops_with_side_effects.push_back(&op);
    }
  }

  OpBuilder builder(fn.getContext());
  // The control tokens generated by the last ControlNodeOp wrapping.  Will be
  // empty until the first ControlNodeOp was generated, then have constant size
  // 1.
  llvm::SmallVector<Value, 1> control_tokens;
  for (auto *op : ops_with_side_effects) {
    // Wrap all side-effect producing/dependent operations in a ControlNodeOp.
    builder.setInsertionPoint(op);
    Location loc = op->getLoc();
    auto outer_op = builder.create<ControlNodeOp>(
        loc, op->getResultTypes(), ControlType::get(op->getContext()),
        control_tokens);
    Region region;
    Block *new_block = new Block;
    region.push_back(new_block);
    builder.setInsertionPointToEnd(&region.front());
    Operation *inner_op = builder.clone(*op);
    builder.create<YieldOp>(loc, inner_op->getResults());
    outer_op.getBody().takeBody(region);
    // Careful: We can't use outer_op.getResults(), because that also includes
    // the control token.
    op->replaceAllUsesWith(outer_op.getOutputs());
    op->erase();
    // Control token is last result of outer_op.
    control_tokens.assign(1, outer_op.getResults().back());
  }
}
}  // namespace

std::unique_ptr<OperationPass<func::FuncOp>> CreatePinOpsWithSideEffectsPass() {
  return std::make_unique<PinOpsWithSideEffectsPass>();
}

static PassRegistration<PinOpsWithSideEffectsPass> pass;

}  // namespace TFL
}  // namespace mlir
