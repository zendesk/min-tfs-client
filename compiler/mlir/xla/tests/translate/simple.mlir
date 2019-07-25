// RUN: tf-mlir-translate -mlir-hlo-to-hlo %s | FileCheck %s

func @main(tensor<4xf32>, tensor<4xf32>) -> tensor<4xf32> {
^bb0(%arg0: tensor<4xf32>, %arg1: tensor<4xf32>):
  %0 = "xla.add"(%arg0, %arg1) : (tensor<4xf32>, tensor<4xf32>) -> tensor<4xf32>
  %1 = "xla.dot"(%0, %arg1) : (tensor<4xf32>, tensor<4xf32>) -> tensor<4xf32>
  return %1 : tensor<4xf32>
}

// CHECK: name: "main
// CHECK: entry_computation_name: "main
// CHECK: computations {
// CHECK: name: "main
// CHECK: instructions {
// CHECK: name: "Arg_
// CHECK: opcode: "parameter"
// CHECK: name: "add
// CHECK: opcode: "add"
// CHECK: name: "dot
// CHECK: opcode: "dot"

