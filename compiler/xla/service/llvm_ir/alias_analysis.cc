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

#include "tensorflow/compiler/xla/service/llvm_ir/alias_analysis.h"

#include <unordered_set>

#include "external/llvm/include/llvm/IR/MDBuilder.h"
#include "tensorflow/compiler/xla/legacy_flags/alias_analysis_flags.h"
#include "tensorflow/compiler/xla/service/llvm_ir/llvm_util.h"
#include "tensorflow/compiler/xla/service/logical_buffer.h"
#include "tensorflow/compiler/xla/types.h"

namespace xla {
namespace llvm_ir {

void AliasAnalysis::AddAliasingInformationToIrArray(const HloInstruction& hlo,
                                                    llvm_ir::IrArray* array) {
  BufferAllocation::Index buffer_index;
  if (hlo.opcode() == HloOpcode::kParameter) {
    // Parameters may alias with each other but may not alias with our temporary
    // buffers.
    buffer_index = kParameterAliasSet;
  } else {
    const std::set<BufferAllocation> allocations =
        assignment_.GetAllocations(&hlo, /*index=*/{});
    if (allocations.empty() || allocations.size() > 1) {
      // Skip HLOs which don't have buffers a buffer assigned or for which the
      // buffer can't be determined statically. We cannot determine their
      // aliasing properties in these cases.
      return;
    }
    buffer_index = allocations.begin()->index();
  }

  llvm::MDNode*& alias_scope_md = alias_scope_metadata_[buffer_index];
  if (alias_scope_md == nullptr) {
    alias_scope_md =
        GetAliasScopeMetadataForBuffer(buffer_index, GetAliasDomain());
  }
  array->AddAliasScopeMetadata(alias_scope_md);

  llvm::MDNode*& noalias_md = noalias_metadata_[buffer_index];
  if (noalias_md == nullptr) {
    noalias_md = GetNoaliasMetadataForBuffer(buffer_index, GetAliasDomain(),
                                             assignment_, hlo);
  }
  array->AddNoaliasMetadata(noalias_md);

  // Parameters of the entry computation are never stored to, loading from a
  // parameter pointer should always return the same result within a loop.
  if (hlo.opcode() == HloOpcode::kParameter) {
    const std::vector<HloInstruction*>& parameter_instructions =
        module_.entry_computation()->parameter_instructions();
    if (std::find(parameter_instructions.begin(), parameter_instructions.end(),
                  &hlo) != parameter_instructions.end()) {
      array->AddInvariantLoad(llvm::MDNode::get(*context_, /*MDs=*/{}));
    }
  }
}

llvm::MDNode* AliasAnalysis::GetAliasDomain() {
  llvm::MDBuilder metadata_builder(*context_);
  if (alias_domain_ == nullptr) {
    alias_domain_ = metadata_builder.createAnonymousAliasScopeDomain();
  }
  return alias_domain_;
}

llvm::MDNode* AliasAnalysis::GetAliasScopeMetadataForBuffer(
    BufferAllocation::Index buffer_index, llvm::MDNode* domain) {
  legacy_flags::AliasAnalysisFlags* flags =
      legacy_flags::GetAliasAnalysisFlags();
  if (!flags->xla_emit_alias_scope) {
    return nullptr;
  }

  // While we could synthesize an alias.scope, doing so is not more profitable
  // than LLVM's default behavior.
  if (buffer_index == kParameterAliasSet) {
    return nullptr;
  }

  llvm::MDBuilder metadata_builder(domain->getContext());
  llvm::MDNode* scope = metadata_builder.createAliasScope(
      AsStringRef(tensorflow::strings::StrCat("buffer: ", buffer_index)),
      domain);
  llvm::MDNode* scope_list = llvm::MDNode::get(domain->getContext(), scope);
  return scope_list;
}

llvm::MDNode* AliasAnalysis::GetNoaliasMetadataForBuffer(
    BufferAllocation::Index buffer_index, llvm::MDNode* domain,
    const BufferAssignment& assignment, const HloInstruction& hlo) {
  legacy_flags::AliasAnalysisFlags* flags =
      legacy_flags::GetAliasAnalysisFlags();
  if (!flags->xla_emit_alias_scope) {
    return nullptr;
  }

  // We want to construct a list of buffers which:
  //
  // 1. Do not alias the given buffer.
  // 2. Will plausibly be used in the vicinity of the given buffer.
  //
  // Making the noalias set overly large will result in either a massive
  // slowdown in LLVM or LLVM will just ignore the noalias set.
  //
  // A plausible list of instructions are:
  // 1. Users of the given hlo.
  // 2. Operands of users of the given hlo.
  // 3. Operands of the given hlo.
  //
  // This set can be increased as we need. For now only consider top-level
  // buffers (index = {}) not buffers nested within the instruction's
  // operands/output which are not typically touched.
  std::vector<const LogicalBuffer*> worklist;
  auto add_buffers_to_worklist =
      [&worklist, &assignment](const HloInstruction* instruction) {
        for (const LogicalBuffer* buffer :
             assignment.GetSourceBuffers(instruction, /*index=*/{})) {
          worklist.push_back(buffer);
        }
      };

  for (HloInstruction* user : hlo.users()) {
    add_buffers_to_worklist(user);
    for (HloInstruction* operand : user->operands()) {
      add_buffers_to_worklist(operand);
    }
  }

  add_buffers_to_worklist(&hlo);
  for (HloInstruction* operand : hlo.operands()) {
    add_buffers_to_worklist(operand);
  }

  std::unordered_set<BufferAllocation::Index> buffers;
  for (const LogicalBuffer* buffer : worklist) {
    // Skip buffers which cannot be added to the noalias set.
    if (!assignment.HasAllocation(*buffer) ||
        buffer->instruction()->opcode() == HloOpcode::kParameter) {
      continue;
    }
    BufferAllocation::Index noalias_index =
        assignment.GetAssignedAllocation(*buffer).index();
    // Our buffer must not noalias itself.
    if (noalias_index != buffer_index) {
      buffers.insert(noalias_index);
      // Some instructions have too many operands, causing the noalias set to be
      // too large. To reduce compilation time (b/31901575), truncate noalias
      // sets to at most 500 elements.
      //
      // Future work: improvements to LLVM's scoped AA that avoid creating a
      // MDNode set for every alias query can help to reduce the compilation
      // time as well.
      constexpr int kMaxNoAliasSetSize = 500;
      if (buffers.size() >= kMaxNoAliasSetSize) {
        break;
      }
    }
  }

  // Don't bother constructing a noalias metadata node if it would be empty.
  if (buffers.empty()) {
    return nullptr;
  }

  llvm::MDBuilder metadata_builder(domain->getContext());
  std::vector<llvm::Metadata*> scopes;
  for (BufferAllocation::Index noalias_index : buffers) {
    llvm::MDNode* scope = metadata_builder.createAliasScope(
        AsStringRef(tensorflow::strings::StrCat("buffer: ", noalias_index)),
        domain);
    scopes.push_back(scope);
  }
  llvm::MDNode* noalias_list =
      llvm::MDNode::get(domain->getContext(), AsArrayRef(scopes));
  return noalias_list;
}

}  // namespace llvm_ir
}  // namespace xla
