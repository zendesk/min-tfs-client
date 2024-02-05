/* Copyright 2019 Google Inc. All Rights Reserved.

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

#include "mlir/Dialect/Shape/IR/Shape.h"  // from @llvm-project
#include "mlir/InitAllDialects.h"  // from @llvm-project
#include "mlir/InitAllPasses.h"  // from @llvm-project
#include "mlir/Tools/mlir-opt/MlirOptMain.h"  // from @llvm-project
#include "mlir/Transforms/Passes.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/init_mlir.h"
#include "tensorflow/compiler/mlir/lite/ir/tfl_ops.h"
#include "tensorflow/compiler/mlir/tensorflow/dialect_registration.h"
#include "tensorflow/compiler/mlir/tensorflow/transforms/passes.h"
#include "tensorflow/compiler/mlir/tfrt/ir/mlrt/mlrt_dialect.h"
#include "tensorflow/compiler/mlir/tfrt/ir/mlrt/tf_mlrt_ops.h"
#include "tensorflow/compiler/mlir/tfrt/ir/tfrt_fallback.h"
#include "tensorflow/compiler/mlir/tfrt/ir/tfrt_fallback_async.h"
#include "tensorflow/compiler/mlir/tfrt/ir/tfrt_fallback_sync.h"
#include "tensorflow/compiler/mlir/tfrt/transforms/gpu_passes.h"
#include "tensorflow/compiler/mlir/tfrt/transforms/mlrt/passes.h"
#include "tensorflow/compiler/mlir/tfrt/transforms/passes.h"
#include "xla/mlir_hlo/gml_st/IR/gml_st_ops.h"
#include "xla/mlir_hlo/gml_st/transforms/passes.h"
#include "xla/mlir_hlo/mhlo/IR/hlo_ops.h"
#include "tensorflow/core/platform/init_main.h"
#include "tfrt/init_tfrt_dialects.h"  // from @tf_runtime

int main(int argc, char **argv) {
  tensorflow::InitMlir y(&argc, &argv);

  mlir::registerAllPasses();
  mlir::registerInlinerPass();

  mlir::registerTensorFlowPasses();

  mlir::gml_st::registerGmlStPasses();

  tensorflow::mlrt_compiler::RegisterMlrtPasses();

  mlir::DialectRegistry registry;
  mlir::registerAllDialects(registry);
  mlir::RegisterAllTensorFlowDialects(registry);
  registry.insert<mlir::gml_st::GmlStDialect>();
  registry.insert<mlir::shape::ShapeDialect>();
  registry.insert<mlir::mhlo::MhloDialect>();
  registry.insert<mlir::TFL::TensorFlowLiteDialect>();
  registry.insert<tfrt::fallback::FallbackDialect>();
  registry.insert<tfrt::fallback_async::FallbackAsyncDialect>();
  registry.insert<tfrt::fallback_sync::FallbackSyncDialect>();
  registry.insert<tensorflow::tf_mlrt::TensorflowMlrtDialect,
                  mlrt::compiler::MlrtDialect>();
  tensorflow::RegisterTPUDialects(&registry);
  tensorflow::RegisterGpuDialects(&registry);

  tfrt::RegisterTFRTDialects(registry);
  return failed(
      mlir::MlirOptMain(argc, argv, "TensorFlow TFRT pass driver\n", registry));
}
