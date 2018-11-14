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

#include "tensorflow/compiler/xla/service/llvm_ir/kernel_tiling.h"
#include "tensorflow/compiler/xla/layout_util.h"
#include "tensorflow/compiler/xla/service/llvm_ir/llvm_util.h"
#include "tensorflow/compiler/xla/shape_util.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/core/platform/logging.h"

namespace xla {
namespace llvm_ir {

namespace {
// Returns the indices of the first elements of all consecutive subarrays of the
// given array. For example:
// ConsecutiveSegments({m, m+1, m+2, n, k, k+1}) = {0, 3, 4}
std::vector<size_t> ConsecutiveSegments(absl::Span<const int64> xs) {
  std::vector<size_t> is = {0};
  for (size_t i = 1; i < xs.size(); ++i) {
    if (1 != xs[i] - xs[i - 1]) {
      is.push_back(i);
    }
  }
  return is;
}

// Merges the sequences of dimensions of the given shape which start at the
// given indices `segs`.
Shape MergeDimensions(absl::Span<const size_t> segs, const Shape& shape) {
  std::vector<int64> dimensions;
  for (size_t i = 1; i <= segs.size(); ++i) {
    dimensions.push_back(std::accumulate(
        shape.dimensions().begin() + segs[i - 1],
        shape.dimensions().begin() +
            (segs.size() == i ? shape.dimensions().size() : segs[i]),
        1, std::multiplies<int64>()));
  }
  return ShapeUtil::MakeShapeWithDescendingLayout(shape.element_type(),
                                                  dimensions);
}
}  // namespace

absl::optional<std::vector<int64> > FindTranspose021(const Shape& a,
                                                     const Shape& b) {
  if (!ShapeUtil::CompatibleIgnoringElementType(a, b)) {
    return absl::nullopt;
  }

  std::vector<int64> perm(a.dimensions().size());
  {
    auto layout_a_orig = LayoutUtil::MinorToMajor(a);
    std::vector<int64> layout_a(layout_a_orig.rbegin(), layout_a_orig.rend());
    auto layout_b_orig = LayoutUtil::MinorToMajor(b);
    std::vector<int64> layout_b(layout_b_orig.rbegin(), layout_b_orig.rend());
    for (size_t i = 0; i < perm.size(); ++i) {
      perm[i] = PositionInContainer(layout_b, layout_a[i]);
    }
  }
  auto segs = ConsecutiveSegments(perm);
  if ((3 == segs.size() && 0 == perm[0]) || 2 == segs.size()) {
    Shape norm_a =
        ShapeUtil::MakeShapeWithDescendingLayoutAndSamePhysicalLayout(a);
    Shape reduced_a = MergeDimensions(segs, norm_a);
    auto reduced_a_dims = reduced_a.dimensions();
    std::vector<int64> dims_021;
    if (2 == segs.size()) {
      // The logical component-0 is of size one.
      dims_021 = {1, reduced_a_dims[1], reduced_a_dims[0]};
    } else {
      dims_021 = {reduced_a_dims[0], reduced_a_dims[2], reduced_a_dims[1]};
    }

    return dims_021;
  }

  return absl::nullopt;
}

IrArray::Index GetUnreducedOutputIndex(
    const IrArray::Index& reduced_output_index,
    const Shape& reduced_output_shape, const Shape& unreduced_output_shape,
    llvm::IRBuilder<>* b) {
  auto bounds = reduced_output_shape.dimensions();
  auto minor_to_major = reduced_output_shape.layout().minor_to_major();
  llvm::Value* linear_index = reduced_output_index.GetConstantWithIndexType(0);
  int64 multiplier = 1;
  for (int i = 0; i < reduced_output_index.size(); ++i) {
    int64 dim = minor_to_major[i];
    llvm::Value* addend =
        b->CreateMul(reduced_output_index[dim],
                     reduced_output_index.GetConstantWithIndexType(multiplier),
                     "linearizing",
                     /*HasNUW=*/true, /*HasNSW=*/true);
    linear_index = b->CreateAdd(linear_index, addend, "",
                                /*HasNUW=*/true, /*HasNSW=*/true);
    multiplier *= bounds[dim];
  }

  return IrArray::Index(linear_index, unreduced_output_shape, b);
}

KernelMappingScheme::KernelMappingScheme(
    absl::Span<const int64> dims_in_elems, int64 tile_size_y, int64 tile_size_x,
    absl::Span<const int64> req_block_sizes, int64 num_threads_y,
    int64 num_threads_x, llvm::IRBuilder<>* b)
    : b_(b),
      dims_in_elems_(dims_in_elems),
      tile_sizes_{1, tile_size_y, tile_size_x},
      num_threads_x_(num_threads_x),
      num_threads_y_(num_threads_y) {
  DCHECK_EQ(dims_in_elems_.size(), 3);
  DCHECK_EQ(req_block_sizes.size(), 3);

  DCHECK_EQ(tile_size_y % num_threads_y_, 0);
  DCHECK_EQ(tile_size_x % num_threads_x_, 0);

  dims_in_tiles_ = ElementWiseCeilOfRatio<int64>(dims_in_elems_, tile_sizes_);
  block_sizes_.reserve(req_block_sizes.size());
  absl::c_transform(req_block_sizes, dims_in_tiles_,
                    std::back_inserter(block_sizes_),
                    [](const int64 requested_size, const int64 max_size) {
                      return std::min(requested_size, max_size);
                    });
  dims_in_blocks_ = ElementWiseCeilOfRatio<int64>(dims_in_tiles_, block_sizes_);

  VLOG(10) << "dims_in_elems_ = [" << absl::StrJoin(dims_in_elems_, ",") << "]";
  VLOG(10) << "dims_in_tiles_ = [" << absl::StrJoin(dims_in_tiles_, ",") << "]";
  VLOG(10) << "dims_in_blocks_ = [" << absl::StrJoin(dims_in_blocks_, ",")
           << "]";
}

IrArray::Index KernelMappingScheme::GetReshapedOutputIndex(
    const IrArray::Index& output_index, const Shape& reshaped_output_shape) {
  DCHECK_EQ(output_index.size(), dims_in_elems_.size());
  Shape output_shape = ShapeUtil::MakeShapeWithDescendingLayout(
      reshaped_output_shape.element_type(), GetDimensionsInElements());
  return llvm_ir::GetUnreducedOutputIndex(output_index, output_shape,
                                          reshaped_output_shape, b_);
}

IrArray::Index KernelMappingScheme::EmitBlockIndex(llvm::Type* index_ty) {
  llvm::Value* block_id = llvm_ir::EmitCallToIntrinsic(
      llvm::Intrinsic::nvvm_read_ptx_sreg_ctaid_x, {}, {}, b_);
  llvm_ir::AddRangeMetadata(0, GetNumberOfBlocks(),
                            llvm::cast<llvm::Instruction>(block_id));
  llvm::Value* linear_block_id =
      b_->CreateIntCast(block_id, index_ty, /*isSigned=*/true, "block.id.x");
  return IrArray::Index(linear_block_id,
                        ShapeUtil::MakeShapeWithDescendingLayout(
                            PRED /*arbitrary*/, dims_in_blocks_),
                        b_);
}

IrArray::Index KernelMappingScheme::GetTileIndexForBlockOrigin(
    const IrArray::Index& block_index) {
  IrArray::Index tile_index = block_index;
  for (int i = 0; i < block_sizes_.size(); ++i) {
    tile_index[i] = b_->CreateMul(
        block_index[i],
        llvm::ConstantInt::get(block_index[i]->getType(), block_sizes_[i]),
        "block_origin." + std::to_string(i));
  }
  return tile_index;
}

IrArray::Index KernelMappingScheme::GetElementIndexForTileOrigin(
    const IrArray::Index& tile_index) {
  IrArray::Index elem_index = tile_index;
  for (int i = DimY; i < DimTot; ++i) {
    elem_index[i] =
        b_->CreateMul(tile_index[i],
                      llvm::ConstantInt::get(tile_index[i]->getType(),
                                             GetTileSizeForDimension(i)),
                      "tile_origin." + std::to_string(i));
  }
  return elem_index;
}

llvm::GlobalVariable* KernelMappingScheme::GetSharedMemoryBufferForElementType(
    llvm::Type* elem_ty, absl::string_view buffer_name) {
  // If shared memory tranpose is needed, we use square tiles.
  CHECK_EQ(GetTileSizeForDimensionX(), GetTileSizeForDimensionY());

  // For Nvidia GPUs, the warp size is 32 threads and the shared memory bank is
  // organized into 32-way. We usually use the warp size or a multiplier or a
  // the warp size as the size for tiling. This may cause all elements in the
  // same column of a tile use the same memory bank and therefore shared memory
  // bank conflicts. Adding 1 to the minor dimension of the shared memory buffer
  // can reduce such shared memory bank conflicts.
  llvm::Type* buffer_type = llvm::ArrayType::get(
      llvm::ArrayType::get(elem_ty, GetTileSizeForDimension(DimX) + 1),
      GetTileSizeForDimension(DimY));
  return llvm_ir::AllocateSharedMemoryTile(b_->GetInsertBlock()->getModule(),
                                           buffer_type, buffer_name);
}

std::tuple<llvm::Value*, llvm::Value*>
KernelMappingScheme::EmitThreadYXCoordinate(llvm::Type* index_ty) {
  // Calculate (y, x) coordinate of the thread in the 2D view of thread block
  // defined by (num_thread_y, num_thread_x) from thread_id.
  llvm::CallInst* thread_id_raw = llvm_ir::EmitCallToIntrinsic(
      llvm::Intrinsic::nvvm_read_ptx_sreg_tid_x, {}, {}, b_);
  llvm_ir::AddRangeMetadata(0, GetThreadsPerTile(), thread_id_raw);
  llvm::Value* thread_id_int =
      b_->CreateIntCast(thread_id_raw, index_ty,
                        /*isSigned=*/true, "thread.id.x");
  llvm::Value* num_thread_x =
      llvm::ConstantInt::get(index_ty, GetNumberOfThreadsForDimensionX());
  llvm::Value* x = b_->CreateURem(thread_id_int, num_thread_x);
  llvm::Value* y = b_->CreateUDiv(thread_id_int, num_thread_x);
  return std::make_tuple(y, x);
}

}  // namespace llvm_ir
}  // namespace xla
