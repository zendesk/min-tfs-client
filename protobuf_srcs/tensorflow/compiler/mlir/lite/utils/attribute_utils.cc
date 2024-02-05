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

#include "mlir/IR/BuiltinAttributes.h"  // from @llvm-project
#include "mlir/IR/BuiltinTypes.h"  // from @llvm-project

namespace mlir {
namespace TFL {

FloatAttr ExtractSingleElementAsFloat(ElementsAttr attr) {
  if (attr.getShapedType().getNumElements() != 1 ||
      !attr.getShapedType().getElementType().isa<FloatType>()) {
    return {};
  }
  return attr.getSplatValue<FloatAttr>();
}

FloatAttr GetSingleElementAsFloatOrSelf(Attribute attr) {
  if (auto m = attr.dyn_cast_or_null<ElementsAttr>()) {
    return ExtractSingleElementAsFloat(m);
  } else {
    return attr.dyn_cast_or_null<FloatAttr>();
  }
}

IntegerAttr ExtractSingleElementAsInteger(ElementsAttr attr) {
  if (attr.getShapedType().getNumElements() != 1 ||
      !attr.getShapedType().getElementType().isSignlessInteger()) {
    return {};
  }
  return attr.getSplatValue<IntegerAttr>();
}

}  // namespace TFL
}  // namespace mlir
