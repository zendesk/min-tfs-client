// RUN: tf-quant-opt %s -split-input-file -quant-insert-quantized-functions -quant-quantize-composite-functions='target-opset=XLA' | FileCheck %s

module {
  func.func @conv_with_single_layer(%arg0: tensor<1x2x2x3xf32>) -> (tensor<*xf32>) {
    %cst = "tf.Const"() {value = dense<[[[[1.600000e-01, 1.000000e-01], [5.100000e-01, 5.400000e-01], [-5.000000e-01, 4.100000e-01]], [[-3.500000e-01, 5.000000e-02], [-0.00999999977, 1.600000e-01], [-4.800000e-01, -2.400000e-01]]], [[[-3.500000e-01, -2.100000e-01], [-1.400000e-01, -2.000000e-02], [4.800000e-01, 3.500000e-01]], [[-1.900000e-01, 3.200000e-01], [0.00999999977, -7.000000e-02], [2.000000e-01, -4.000000e-02]]]]> : tensor<2x2x3x2xf32>} : () -> tensor<2x2x3x2xf32>
    %cst_0 = "tf.Const"() {value = dense<[-2.000000e+00, 3.000000e+00]> : tensor<2xf32>} : () -> tensor<2xf32>
    %0 = "quantfork.qcast"(%cst) : (tensor<2x2x3x2xf32>) -> tensor<2x2x3x2x!quant.uniform<i8<-127:127>:f32:3, {4.000000e-03,5.000000e-03}>>
    %1 = "quantfork.dcast"(%0) : (tensor<2x2x3x2x!quant.uniform<i8<-127:127>:f32:3, {4.000000e-03,5.000000e-03}>>) -> tensor<*xf32>
    %2 = "quantfork.qcast"(%arg0) : (tensor<1x2x2x3xf32>) -> tensor<1x2x2x3x!quant.uniform<i8:f32, 8.000000e-03>>
    %3 = "quantfork.dcast"(%2) : (tensor<1x2x2x3x!quant.uniform<i8:f32, 8.000000e-03>>) -> tensor<*xf32>
    %4 = "tf.PartitionedCall"(%3, %1, %cst_0) {_tfl_quant_trait = "fully_quantizable", config = "", config_proto = "", executor_type = "", f = @composite_conv2d_with_bias_and_relu6_fn_1} : (tensor<*xf32>, tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
    %5 = "quantfork.qcast"(%4) : (tensor<*xf32>) -> tensor<*x!quant.uniform<i8:f32, 5.000000e-02:-1>>
    %6 = "quantfork.dcast"(%5) : (tensor<*x!quant.uniform<i8:f32, 5.000000e-02:-1>>) -> tensor<*xf32>
    func.return %6 : tensor<*xf32>
  }
  func.func private @composite_conv2d_with_bias_and_relu6_fn_1(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>, %arg2: tensor<2xf32>) -> tensor<*xf32> attributes {tf_quant.composite_function} {
    %0 = "tf.Conv2D"(%arg0, %arg1) {attr_map = "0:strides,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations", data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "VALID", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true} : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
    %1 = "tf.BiasAdd"(%0, %arg2) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
    %2 = "tf.Relu6"(%1) : (tensor<*xf32>) -> tensor<*xf32>
    func.return %2 : tensor<*xf32>
  }

// CHECK-LABEL: func @conv_with_single_layer

// CHECK: %[[quantize:.*]] = "tf.PartitionedCall"(%arg0
// CHECK-SAME: f = @quantize_i8
// CHECK: %[[conv_quant:.*]] = "tf.PartitionedCall"(%[[quantize]]
// CHECK-SAME: f = @quantized_conv2d_with_bias_and_relu6_float_output_fn_0
// CHECK-SAME: (tensor<1x2x2x3xi8>, tensor<2x2x3x2xi8>, tensor<2xi32>, tensor<f32>, tensor<i32>, tensor<2xf32>, tensor<2xi32>, tensor<2xf32>, tensor<2xi32>, tensor<f32>, tensor<i32>) -> tensor<*xf32>
// CHECK: return %[[conv_quant]]

// CHECK-LABEL: func private @quantized_conv2d_with_bias_and_relu6_float_output_fn_0
// CHECK-SAME: (%arg0: tensor<1x2x2x3xi8>, %arg1: tensor<2x2x3x2xi8>, %arg2: tensor<2xi32>, %arg3: tensor<f32>, %arg4: tensor<i32>, %arg5: tensor<2xf32>, %arg6: tensor<2xi32>, %arg7: tensor<2xf32>, %arg8: tensor<2xi32>, %arg9: tensor<f32>, %arg10: tensor<i32>) -> tensor<*xf32>
// CHECK:      %[[CONV2D_0:.*]] = "tf.Conv2D"
// CHECK-SAME: {dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "VALID", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true}

// CHECK: -------- Quantization Summary --------
// CHECK: Number of quantized layers in the model
// CHECK: --------------------------------
// CHECK: Name    Count/Total
// CHECK: ================================
// CHECK: Conv2D  1/1

// CHECK: Number of quantized layers with quantized outputs: 0/1
// CHECK: Number of quantize layers added: 1
// CHECK: Number of dequantize layers added: 0
}

// -----

module attributes {tf.versions = {bad_consumers = [], min_consumer = 12 : i32, producer = 1219 : i32}, tf_saved_model.semantics} {
  func.func @conv_with_two_layers(%arg0: tensor<1x3x4x3xf32> {tf_saved_model.index_path = ["input"]}) -> (tensor<1x3x2x2xf32> {tf_saved_model.index_path = ["output"]}) attributes {tf.entry_function = {control_outputs = "", inputs = "serving_default_input:0", outputs = "PartitionedCall:0"}, tf_saved_model.exported_names = ["serving_default"]} {
    %cst = "tf.Const"() {device = "", value = dense<[[[[-0.315365672, 0.27481091], [0.0901821703, -0.382271349], [-0.105572946, -0.354302853]], [[-0.47703138, -0.307006568], [0.306320101, -0.209111646], [0.252869487, 0.449634969]], [[0.167675957, 0.042408213], [-0.332338423, -0.397738814], [0.290657759, 0.460783273]]], [[[0.0693112761, 0.231933162], [0.477371335, -0.0718854442], [-0.398417652, 0.449998438]], [[0.0494867712, -0.241692379], [-0.363851488, 0.0586083047], [0.466867805, 0.0364450105]], [[0.256431073, 0.44932279], [0.0775043964, -0.192745745], [0.185018882, 0.463297218]]]]> : tensor<2x3x3x2xf32>} : () -> tensor<2x3x3x2xf32>
    %cst_0 = "tf.Const"() {device = "", value = dense<[[[[0.211401448, 0.205456927], [0.418355644, -0.314615548]], [[0.493921608, -0.101286061], [-0.16083248, -0.0546654463]], [[-0.157245964, 0.419805884], [-0.0499645844, 3.726310e-01]]], [[[-0.353424132, 0.361233443], [0.391344249, -0.364820778]], [[-0.476781279, -0.180014133], [-0.302823931, 0.199466437]], [[-0.385851651, 0.0372837223], [-0.0986057966, -0.0732412189]]]]> : tensor<2x3x2x2xf32>} : () -> tensor<2x3x2x2xf32>
    %0 = "quantfork.qcast"(%arg0) : (tensor<1x3x4x3xf32>) -> tensor<1x3x4x3x!quant.uniform<i8:f32, 0.0011764706057660721:-43>>
    %1 = "quantfork.dcast"(%0) : (tensor<1x3x4x3x!quant.uniform<i8:f32, 0.0011764706057660721:-43>>) -> tensor<1x3x4x3xf32>
    %2 = "tf.PartitionedCall"(%1, %cst) {_tfl_quant_trait = "fully_quantizable", config = "", config_proto = "", executor_type = "", f = @composite_conv2d_with_relu_fn_1} : (tensor<1x3x4x3xf32>, tensor<2x3x3x2xf32>) -> tensor<1x3x2x2xf32>
    %3 = "quantfork.qcast"(%2) : (tensor<1x3x2x2xf32>) -> tensor<1x3x2x2x!quant.uniform<i8:f32, 0.0027450981093387976:-19>>
    %4 = "quantfork.dcast"(%3) : (tensor<1x3x2x2x!quant.uniform<i8:f32, 0.0027450981093387976:-19>>) -> tensor<1x3x2x2xf32>
    %5 = "tf.PartitionedCall"(%4, %cst_0) {_tfl_quant_trait = "fully_quantizable", config = "", config_proto = "", executor_type = "", f = @composite_conv2d_fn_1} : (tensor<1x3x2x2xf32>, tensor<2x3x2x2xf32>) -> tensor<1x3x2x2xf32>
    %6 = "quantfork.qcast"(%5) : (tensor<1x3x2x2xf32>) -> tensor<1x3x2x2x!quant.uniform<i8:f32, 0.0054901962186775953:-19>>
    %7 = "quantfork.dcast"(%6) : (tensor<1x3x2x2x!quant.uniform<i8:f32, 0.0054901962186775953:-19>>) -> tensor<1x3x2x2xf32>
    return %7 : tensor<1x3x2x2xf32>
  }
  func.func private @composite_conv2d_with_relu_fn_1(%arg0: tensor<1x3x4x3xf32>, %arg1: tensor<2x3x3x2xf32>) -> tensor<1x3x2x2xf32> attributes {tf_quant.composite_function} {
    %0 = "tf.Conv2D"(%arg0, %arg1) {attr_map = "0:strides,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations", data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true} : (tensor<1x3x4x3xf32>, tensor<2x3x3x2xf32>) -> tensor<1x3x2x2xf32>
    %1 = "tf.Relu"(%0) {device = ""} : (tensor<1x3x2x2xf32>) -> tensor<1x3x2x2xf32>
    return %1 : tensor<1x3x2x2xf32>
  }
  func.func private @composite_conv2d_fn_1(%arg0: tensor<1x3x2x2xf32>, %arg1: tensor<2x3x2x2xf32>) -> tensor<1x3x2x2xf32> attributes {tf_quant.composite_function} {
    %0 = "tf.Conv2D"(%arg0, %arg1) {attr_map = "0:strides,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations", data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "SAME", strides = [1, 1, 1, 1], use_cudnn_on_gpu = true} : (tensor<1x3x2x2xf32>, tensor<2x3x2x2xf32>) -> tensor<1x3x2x2xf32>
    return %0 : tensor<1x3x2x2xf32>
  }

// CHECK-LABEL: func @conv_with_two_layers

// CHECK: %[[quantize:.*]] = "tf.PartitionedCall"(%arg0
// CHECK-SAME: f = @quantize_i8
// CHECK: %[[conv_quant:.*]] = "tf.PartitionedCall"(%[[quantize]]
// CHECK-SAME: f = @quantized_conv2d_with_relu_fn_0
// CHECK: %[[conv_quant2:.*]] = "tf.PartitionedCall"(%[[conv_quant]]
// CHECK-SAME: f = @quantized_conv2d_float_output_fn_0
// CHECK: return %[[conv_quant2]]

// CHECK: -------- Quantization Summary --------
// CHECK: Number of quantized layers in the model
// CHECK: --------------------------------
// CHECK: Name    Count/Total
// CHECK: ================================
// CHECK: Conv2D  2/2

// CHECK: Number of quantized layers with quantized outputs: 1/2
// CHECK: Number of quantize layers added: 1
// CHECK: Number of dequantize layers added: 0
}

// -----

module {
  func.func @conv_with_maxpool(%arg0: tensor<1x2x2x3xf32>) -> (tensor<*xf32>) {
    %cst = "tf.Const"() {value = dense<[[[[1.600000e-01, 1.000000e-01], [5.100000e-01, 5.400000e-01], [-5.000000e-01, 4.100000e-01]], [[-3.500000e-01, 5.000000e-02], [-0.00999999977, 1.600000e-01], [-4.800000e-01, -2.400000e-01]]], [[[-3.500000e-01, -2.100000e-01], [-1.400000e-01, -2.000000e-02], [4.800000e-01, 3.500000e-01]], [[-1.900000e-01, 3.200000e-01], [0.00999999977, -7.000000e-02], [2.000000e-01, -4.000000e-02]]]]> : tensor<2x2x3x2xf32>} : () -> tensor<2x2x3x2xf32>
    %cst_0 = "tf.Const"() {value = dense<[-2.000000e+00, 3.000000e+00]> : tensor<2xf32>} : () -> tensor<2xf32>
    %0 = "quantfork.qcast"(%cst) : (tensor<2x2x3x2xf32>) -> tensor<2x2x3x2x!quant.uniform<i8<-127:127>:f32:3, {4.000000e-03,5.000000e-03}>>
    %1 = "quantfork.dcast"(%0) : (tensor<2x2x3x2x!quant.uniform<i8<-127:127>:f32:3, {4.000000e-03,5.000000e-03}>>) -> tensor<*xf32>
    %2 = "quantfork.qcast"(%arg0) : (tensor<1x2x2x3xf32>) -> tensor<1x2x2x3x!quant.uniform<i8:f32, 8.000000e-03>>
    %3 = "quantfork.dcast"(%2) : (tensor<1x2x2x3x!quant.uniform<i8:f32, 8.000000e-03>>) -> tensor<*xf32>
    %4 = "tf.PartitionedCall"(%3, %1, %cst_0) {_tfl_quant_trait = "fully_quantizable", config = "", config_proto = "", executor_type = "", f = @composite_conv2d_with_bias_and_relu6_fn_1} : (tensor<*xf32>, tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
    %5 = "quantfork.qcast"(%4) : (tensor<*xf32>) -> tensor<*x!quant.uniform<i8:f32, 5.000000e-02:-1>>
    %6 = "quantfork.dcast"(%5) : (tensor<*x!quant.uniform<i8:f32, 5.000000e-02:-1>>) -> tensor<*xf32>
    %7 = "tf.MaxPool"(%6) {data_format = "NHWC", ksize = [1, 2, 2, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<*xf32>) -> tensor<*xf32>
    func.return %7 : tensor<*xf32>
  }
  func.func private @composite_conv2d_with_bias_and_relu6_fn_1(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>, %arg2: tensor<2xf32>) -> tensor<*xf32> attributes {tf_quant.composite_function} {
    %0 = "tf.Conv2D"(%arg0, %arg1) {attr_map = "0:strides,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations", data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "VALID", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true} : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
    %1 = "tf.BiasAdd"(%0, %arg2) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
    %2 = "tf.Relu6"(%1) : (tensor<*xf32>) -> tensor<*xf32>
    func.return %2 : tensor<*xf32>
  }

// CHECK-LABEL: func @conv_with_maxpool
// CHECK: %[[quantize:.*]] = "tf.PartitionedCall"(%arg0
// CHECK-SAME: f = @quantize_i8
// CHECK: %[[conv_quant:.*]] = "tf.PartitionedCall"(%[[quantize]]
// CHECK-SAME: f = @quantized_conv2d_with_bias_and_relu6_fn_0
// CHECK-SAME: (tensor<1x2x2x3xi8>, tensor<2x2x3x2xi8>, tensor<2xi32>, tensor<f32>, tensor<i32>, tensor<2xf32>, tensor<2xi32>, tensor<2xf32>, tensor<2xi32>, tensor<f32>, tensor<i32>) -> tensor<*xi8>
// CHECK: %[[maxpool:.*]] = "tf.MaxPool"(%[[conv_quant]]) {data_format = "NHWC", ksize = [1, 2, 2, 1], padding = "VALID", strides = [1, 1, 1, 1]} : (tensor<*xi8>) -> tensor<*xi8>
// CHECK: %[[dequantize:.*]] = "tf.PartitionedCall"(%[[maxpool]]
// CHECK-SAME: f = @dequantize_i8
// CHECK: return %[[dequantize]]

// CHECK: -------- Quantization Summary --------
// CHECK: Number of quantized layers in the model
// CHECK: --------------------------------
// CHECK: Name    Count/Total
// CHECK: ================================
// CHECK: Conv2D  1/1

// CHECK: Number of quantized layers with quantized outputs: 1/1
// CHECK: Number of quantize layers added: 1
// CHECK: Number of dequantize layers added: 1
}

// -----

module attributes {tf.versions = {bad_consumers = [], min_consumer = 12 : i32, producer = 1219 : i32}, tf_saved_model.semantics} {
  func.func @embedding_with_one_float_conv_and_one_quantized_conv(%arg0: tensor<1xi32> {tf_saved_model.index_path = ["input"]}) -> (tensor<1x3x1x1xf32> {tf_saved_model.index_path = ["output"]}) attributes {tf.entry_function = {control_outputs = "", inputs = "serving_default_input:0", outputs = "PartitionedCall:0"}, tf_saved_model.exported_names = ["serving_default"]} {

    %cst = "tf.Const"() {value = dense<0.000000e+00> : tensor<3x3x1024x1xf32>} : () -> tensor<3x3x1024x1xf32>
    %cst_0 = "tf.Const"() {value = dense<0.000000e+00> : tensor<1024x3x4x3xf32>} : () -> tensor<1024x3x4x3xf32>
    %cst_1 = "tf.Const"() {value = dense<0> : tensor<i32>} : () -> tensor<i32>
    %cst_2 = "tf.Const"() {value = dense<0.000000e+00> : tensor<2x3x3x1024xf32>} : () -> tensor<2x3x3x1024xf32>

    %0 = "tf.PartitionedCall"(%cst_0, %arg0, %cst_1) {_tfl_quant_trait = "fully_quantizable", config = "", config_proto = "", executor_type = "", f = @composite_gather_fn_1} : (tensor<1024x3x4x3xf32>, tensor<1xi32>, tensor<i32>) -> tensor<1x3x4x3xf32>
    %1 = "tf.PartitionedCall"(%0, %cst_2) {_tfl_quant_trait = "fully_quantizable", config = "", config_proto = "", executor_type = "", f = @composite_conv2d_fn_2} : (tensor<1x3x4x3xf32>, tensor<2x3x3x1024xf32>) -> tensor<1x3x2x1024xf32>
    %2 = "quantfork.qcast"(%1) : (tensor<1x3x2x1024xf32>) -> tensor<1x3x2x1024x!quant.uniform<i8:f32, 0.0011764706057660721:-43>>
    %3 = "quantfork.dcast"(%2) : (tensor<1x3x2x1024x!quant.uniform<i8:f32, 0.0011764706057660721:-43>>) -> tensor<1x3x2x1024xf32>
    %4 = "tf.PartitionedCall"(%3, %cst) {_tfl_quant_trait = "fully_quantizable", config = "", config_proto = "", executor_type = "", f = @composite_conv2d_fn_1} : (tensor<1x3x2x1024xf32>, tensor<3x3x1024x1xf32>) -> tensor<1x3x1x1xf32>
    %5 = "quantfork.qcast"(%4) : (tensor<1x3x1x1xf32>) -> tensor<1x3x1x1x!quant.uniform<i8:f32, 0.0011764706057660721:-43>>
    %6 = "quantfork.dcast"(%5) : (tensor<1x3x1x1x!quant.uniform<i8:f32, 0.0011764706057660721:-43>>) -> tensor<1x3x1x1xf32>
    return %6 : tensor<1x3x1x1xf32>
  }
  func.func private @composite_gather_fn_1(%arg0: tensor<1024x3x4x3xf32>, %arg1: tensor<1xi32>, %arg2: tensor<i32>) -> tensor<1x3x4x3xf32> attributes {tf_quant.composite_function} {
    %0 = "tf.GatherV2"(%arg0, %arg1, %arg2) {attr_map = "0:batch_dims", batch_dims = 0 : i64, device = ""} : (tensor<1024x3x4x3xf32>, tensor<1xi32>, tensor<i32>) -> tensor<1x3x4x3xf32>
    return %0 : tensor<1x3x4x3xf32>
  }
  func.func private @composite_conv2d_fn_2(%arg0: tensor<1x3x4x3xf32>, %arg1: tensor<2x3x3x1024xf32>) -> tensor<1x3x2x1024xf32> attributes {tf_quant.composite_function} {
    %0 = "tf.Conv2D"(%arg0, %arg1) {attr_map = "0:strides,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations", data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true} : (tensor<1x3x4x3xf32>, tensor<2x3x3x1024xf32>) -> tensor<1x3x2x1024xf32>
    return %0 : tensor<1x3x2x1024xf32>
  }
  func.func private @composite_conv2d_fn_1(%arg0: tensor<1x3x2x1024xf32>, %arg1: tensor<3x3x1024x1xf32>) -> tensor<1x3x1x1xf32> attributes {tf_quant.composite_function} {
    %0 = "tf.Conv2D"(%arg0, %arg1) {attr_map = "0:strides,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations", data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true} : (tensor<1x3x2x1024xf32>, tensor<3x3x1024x1xf32>) -> tensor<1x3x1x1xf32>
    return %0 : tensor<1x3x1x1xf32>
  }

// CHECK-LABEL: func @embedding_with_one_float_conv_and_one_quantized_conv

// CHECK: %[[quantized_gather:.*]] = "tf.PartitionedCall"(
// CHECK-SAME: f = @quantized_gather_hybrid_fn_0
// CHECK: %[[float_conv:.*]] = "tf.PartitionedCall"(%[[quantized_gather]]
// CHECK-SAME: f = @composite_conv2d_fn_2
// CHECK: %[[quantize:.*]] = "tf.PartitionedCall"(%[[float_conv]]
// CHECK-SAME: f = @quantize_i8
// CHECK: %[[quantized_conv:.*]] = "tf.PartitionedCall"(%[[quantize]]
// CHECK-SAME: f = @quantized_conv2d_float_output_fn_0

// CHECK: -------- Quantization Summary --------
// CHECK: Number of quantized layers in the model
// CHECK: --------------------------------
// CHECK: Name    Count/Total
// CHECK: ================================
// CHECK: Gather  1/1
// CHECK: Conv2D  1/2

// CHECK: Number of quantized layers with quantized outputs: 0/2
// CHECK: Number of quantize layers added: 1
// CHECK: Number of dequantize layers added: 0
}

// -----

module attributes {tf.versions = {bad_consumers = [], min_consumer = 12 : i32, producer = 1219 : i32}, tf_saved_model.semantics} {
  func.func @gather_with_float_output(%arg0: tensor<1x3x2x1024xf32> {tf_saved_model.index_path = ["input1"]}, %arg1: tensor<1xi32> {tf_saved_model.index_path = ["input2"]}) -> (tensor<1x3x1x1xf32> {tf_saved_model.index_path = ["output"]}) attributes {tf.entry_function = {control_outputs = "", inputs = "serving_default_input:0", outputs = "PartitionedCall:0"}, tf_saved_model.exported_names = ["serving_default"]} {
    %cst_0 = "tf.Const"() {value = dense<0.000000e+00> : tensor<3x3x1024x1xf32>} : () -> tensor<3x3x1024x1xf32>
    %cst_1 = "tf.Const"() {value = dense<0> : tensor<i32>} : () -> tensor<i32>
    %cst_2 = "tf.Const"() {value = dense<0.000000e+00> : tensor<2x3x3x1024xf32>} : () -> tensor<2x3x3x1024xf32>

    %0 = "quantfork.qcast"(%arg0) : (tensor<1x3x2x1024xf32>) -> tensor<1x3x2x1024x!quant.uniform<i8:f32, 0.0011764706057660721:-43>>
    %1 = "quantfork.dcast"(%0) : (tensor<1x3x2x1024x!quant.uniform<i8:f32, 0.0011764706057660721:-43>>) -> tensor<1x3x2x1024xf32>
    %2 = "tf.PartitionedCall"(%1, %cst_0) {_tfl_quant_trait = "fully_quantizable", config = "", config_proto = "", executor_type = "", f = @composite_conv2d_fn_1} : (tensor<1x3x2x1024xf32>, tensor<3x3x1024x1xf32>) -> tensor<1x3x1x1xf32>
    %3 = "quantfork.qcast"(%2) : (tensor<1x3x1x1xf32>) -> tensor<1x3x1x1x!quant.uniform<i8:f32, 0.0011764706057660721:-43>>
    %4 = "quantfork.dcast"(%3) : (tensor<1x3x1x1x!quant.uniform<i8:f32, 0.0011764706057660721:-43>>) -> tensor<1x3x1x1xf32>
    %5 = "tf.PartitionedCall"(%4, %arg1, %cst_1) {_tfl_quant_trait = "fully_quantizable", config = "", config_proto = "", executor_type = "", f = @composite_gather_fn_1} : (tensor<1x3x1x1xf32>, tensor<1xi32>, tensor<i32>) -> tensor<1x3x1x1xf32>
    %6 = "quantfork.qcast"(%5) : (tensor<1x3x1x1xf32>) -> tensor<1x3x1x1x!quant.uniform<i8:f32, 0.0011764706057660721:-43>>
    %7 = "quantfork.dcast"(%6) : (tensor<1x3x1x1x!quant.uniform<i8:f32, 0.0011764706057660721:-43>>) -> tensor<1x3x1x1xf32>
    return %7 : tensor<1x3x1x1xf32>
  }
  func.func private @composite_gather_fn_1(%arg0: tensor<1x3x1x1xf32>, %arg1: tensor<1xi32>, %arg2: tensor<i32>) -> tensor<1x3x1x1xf32> attributes {tf_quant.composite_function} {
    %0 = "tf.GatherV2"(%arg0, %arg1, %arg2) {attr_map = "0:batch_dims", batch_dims = 0 : i64, device = ""} : (tensor<1x3x1x1xf32>, tensor<1xi32>, tensor<i32>) -> tensor<1x3x1x1xf32>
    return %0 : tensor<1x3x1x1xf32>
  }
  func.func private @composite_conv2d_fn_1(%arg0: tensor<1x3x2x1024xf32>, %arg1: tensor<3x3x1024x1xf32>) -> tensor<1x3x1x1xf32> attributes {tf_quant.composite_function} {
    %0 = "tf.Conv2D"(%arg0, %arg1) {attr_map = "0:strides,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations", data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "SAME", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true} : (tensor<1x3x2x1024xf32>, tensor<3x3x1024x1xf32>) -> tensor<1x3x1x1xf32>
    return %0 : tensor<1x3x1x1xf32>
  }

// CHECK-LABEL: func @gather_with_float_output

// CHECK: %[[quantized_input:.*]] = "tf.PartitionedCall"(
// CHECK-SAME: f = @quantize_i8
// CHECK: %[[quantized_conv:.*]] = "tf.PartitionedCall"(%[[quantized_input]]
// CHECK-SAME: f = @quantized_conv2d_fn_0
// CHECK: %[[quantized_gather:.*]] = "tf.PartitionedCall"(%[[quantized_conv]]
// CHECK-SAME: f = @quantized_gather_float_output_fn_0
// return %[[quantized_gather]] : tensor<1x3x1x1xf32>

// CHECK: -------- Quantization Summary --------
// CHECK: Number of quantized layers in the model
// CHECK: --------------------------------
// CHECK: Name    Count/Total
// CHECK: ================================
// CHECK: Gather  1/1
// CHECK: Conv2D  1/1

// CHECK: Number of quantized layers with quantized outputs: 1/2
// CHECK: Number of quantize layers added: 1
// CHECK: Number of dequantize layers added: 0
}

// -----

module {
  func.func @conv_with_dump(%arg0: tensor<*xf32>) -> tensor<*xf32> {
    %cst = "tf.Const"() {device = "", value = dense<[-0.0192535277, -5.998660e-03]> : tensor<2xf32>} : () -> tensor<2xf32>
    %cst_0 = "tf.Const"() {device = "", value = dense<[[[[-0.282878935, -0.211567819], [-0.248810023, -0.0989695191]], [[0.400888503, 0.0803082585], [-0.0671417713, -0.23301053]]], [[[0.345862567, 0.311298311], [-0.595954239, 0.202630222]], [[-0.606417357, -0.257358253], [-0.3036502, -0.35013032]]]]> : tensor<2x2x2x2xf32>} : () -> tensor<2x2x2x2xf32>
    %cst_1 = "tf.Const"() {device = "", value = dense<[-0.0291469581, 0.0106381178]> : tensor<2xf32>} : () -> tensor<2xf32>
    %cst_2 = "tf.Const"() {device = "", value = dense<[[[[0.208403707, 0.478067577], [0.593097508, -0.305721074]], [[-0.114346057, 0.583530128], [0.211413622, -0.606618404]]], [[[0.314416587, -0.260997623], [-0.375806928, 0.0813755393]], [[-0.208318114, 0.275989294], [-3.469230e-01, -0.406548172]]]]> : tensor<2x2x2x2xf32>} : () -> tensor<2x2x2x2xf32>
    %0 = "quantfork.stats"(%arg0) {layerStats = dense<[1.878980e-02, 0.988373816]> : tensor<2xf32>} : (tensor<*xf32>) -> tensor<*xf32>
    %1 = "tf.PartitionedCall"(%0, %cst_0, %cst) {_tfl_quant_trait = "fully_quantizable", config = "", config_proto = "", device = "", executor_type = "", f = @composite_conv2d_with_bias_and_relu6_fn_20} : (tensor<*xf32>, tensor<2x2x2x2xf32>, tensor<2xf32>) -> tensor<*xf32>
    %2 = "quantfork.stats"(%1) {layerStats = dense<[0.000000e+00, 0.36084348]> : tensor<2xf32>} : (tensor<*xf32>) -> tensor<*xf32>
    "tf.DumpTensor"(%2) {device = "", enabled = true, file_name = "quantized_tensor_data.pb", func_name = "conv_with_dump", log_dir_path = "/tmp/dumps/composite_conv2d_with_bias_and_relu6_fn_2", node_name = "Conv2D"} : (tensor<*xf32>) -> ()
    %3 = "tf.PartitionedCall"(%2, %cst_2, %cst_1) {_tfl_quant_trait = "fully_quantizable", config = "", config_proto = "", device = "", executor_type = "", f = @composite_conv2d_with_bias_and_relu6_fn_10} : (tensor<*xf32>, tensor<2x2x2x2xf32>, tensor<2xf32>) -> tensor<*xf32>
    %4 = "quantfork.stats"(%3) {layerStats = dense<[0.000000e+00, 0.18486841]> : tensor<2xf32>} : (tensor<*xf32>) -> tensor<*xf32>
    %5 = "tf.Identity"(%4) {device = ""} : (tensor<*xf32>) -> tensor<*xf32>
    %6 = "quantfork.stats"(%5) {layerStats = dense<[0.000000e+00, 0.18486841]> : tensor<2xf32>} : (tensor<*xf32>) -> tensor<*xf32>
    %7 = "tf.PartitionedCall"(%2, %cst_2, %cst_1) {config = "", config_proto = "", device = "", executor_type = "", f = @composite_conv2d_with_bias_and_relu6_fn_1_00} : (tensor<*xf32>, tensor<2x2x2x2xf32>, tensor<2xf32>) -> tensor<*xf32>
    %8 = "quantfork.stats"(%7) {layerStats = dense<[0.000000e+00, 0.18486841]> : tensor<2xf32>} : (tensor<*xf32>) -> tensor<*xf32>
    %9 = "tf.PartitionedCall"(%0, %cst_0, %cst) {config = "", config_proto = "", device = "", executor_type = "", f = @composite_conv2d_with_bias_and_relu6_fn_2_00} : (tensor<*xf32>, tensor<2x2x2x2xf32>, tensor<2xf32>) -> tensor<*xf32>
    %10 = "quantfork.stats"(%9) {layerStats = dense<[0.000000e+00, 0.36084348]> : tensor<2xf32>} : (tensor<*xf32>) -> tensor<*xf32>
    "tf.DumpTensor"(%10) {device = "", enabled = true, file_name = "unquantized_tensor_data.pb", func_name = "conv_with_dump", log_dir_path = "/tmp/dumps/composite_conv2d_with_bias_and_relu6_fn_2", node_name = "Conv2D"} : (tensor<*xf32>) -> ()
    "tf.DumpTensor"(%4) {device = "", enabled = true, file_name = "quantized_tensor_data.pb", func_name = "conv_with_dump", log_dir_path = "/tmp/dumps/composite_conv2d_with_bias_and_relu6_fn_1", node_name = "Conv2D_1"} : (tensor<*xf32>) -> ()
    "tf.DumpTensor"(%8) {device = "", enabled = true, file_name = "unquantized_tensor_data.pb", func_name = "conv_with_dump", log_dir_path = "/tmp/dumps/composite_conv2d_with_bias_and_relu6_fn_1", node_name = "Conv2D_1"} : (tensor<*xf32>) -> ()
    func.return %6 : tensor<*xf32>
  }

  func.func private @composite_conv2d_with_bias_and_relu6_fn_10(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>, %arg2: tensor<2xf32>) -> tensor<*xf32> attributes {tf_quant.composite_function} {
    %0 = "tf.Conv2D"(%arg0, %arg1) {attr_map = "0:strides,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations", data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "VALID", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true} : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
    %1 = "tf.BiasAdd"(%0, %arg2) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
    %2 = "tf.Relu6"(%1) : (tensor<*xf32>) -> tensor<*xf32>
    func.return %2 : tensor<*xf32>
  }
  func.func private @composite_conv2d_with_bias_and_relu6_fn_1_00(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>, %arg2: tensor<2xf32>) -> tensor<*xf32> attributes {tf_quant.composite_function} {
    %0 = "tf.Conv2D"(%arg0, %arg1) {attr_map = "0:strides,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations", data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "VALID", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true} : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
    %1 = "tf.BiasAdd"(%0, %arg2) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
    %2 = "tf.Relu6"(%1) : (tensor<*xf32>) -> tensor<*xf32>
    func.return %2 : tensor<*xf32>
  }
  func.func private @composite_conv2d_with_bias_and_relu6_fn_20(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>, %arg2: tensor<2xf32>) -> tensor<*xf32> attributes {tf_quant.composite_function} {
    %0 = "tf.Conv2D"(%arg0, %arg1) {attr_map = "0:strides,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations", data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "VALID", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true} : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
    %1 = "tf.BiasAdd"(%0, %arg2) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
    %2 = "tf.Relu6"(%1) : (tensor<*xf32>) -> tensor<*xf32>
    func.return %2 : tensor<*xf32>
  }
  func.func private @composite_conv2d_with_bias_and_relu6_fn_2_00(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>, %arg2: tensor<2xf32>) -> tensor<*xf32> attributes {tf_quant.composite_function} {
    %0 = "tf.Conv2D"(%arg0, %arg1) {attr_map = "0:strides,1:use_cudnn_on_gpu,2:padding,3:explicit_paddings,4:dilations", data_format = "NHWC", device = "", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "VALID", strides = [1, 1, 2, 1], use_cudnn_on_gpu = true} : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
    %1 = "tf.BiasAdd"(%0, %arg2) {data_format = "NHWC", device = ""} : (tensor<*xf32>, tensor<2xf32>) -> tensor<*xf32>
    %2 = "tf.Relu6"(%1) : (tensor<*xf32>) -> tensor<*xf32>
    func.return %2 : tensor<*xf32>
  }

// CHECK-LABE: @conv_with_dump
// CHECK-DAG: %[[w0_float:.*]] = "tf.Const"() {value = dense<{{\[\[\[\[}}-0.282878935, -0.211567819
// CHECK-DAG: %[[b0_float:.*]] = "tf.Const"() {value = dense<[-0.0192535277, -5.998660e-03]> : tensor<2xf32>} : () -> tensor<2xf32>
// CHECK-DAG: %[[w1_float:.*]] = "tf.Const"() {value = dense<{{\[\[\[\[}}0.208403707, 0.478067577
// CHECK-DAG: %[[b1_float:.*]] = "tf.Const"() {value = dense<[-0.0291469581, 0.0106381178]> : tensor<2xf32>} : () -> tensor<2xf32>
// CHECK-DAG: %[[w0_quantized:.*]] = "tf.Const"() {value = dense<{{\[\[\[\[}}-59, -44
// CHECK-DAG: %[[b0_quantized:.*]] = "tf.Const"() {value = dense<[-1040, -324]> : tensor<2xi32>} : () -> tensor<2xi32>
// CHECK-DAG: %[[w1_quantized:.*]] = "tf.Const"() {value = dense<{{\[\[\[\[}}44, 100
// CHECK-DAG: %[[b1_quantized:.*]] = "tf.Const"() {value = dense<[-4312, 1574]> : tensor<2xi32>} : () -> tensor<2xi32>
// CHECK-DAG: %[[in_scale:.*]] = "tf.Const"() {value = dense<0.00387597573> : tensor<f32>} : () -> tensor<f32>
// CHECK-DAG: %[[in_out_zp:.*]] = "tf.Const"() {value = dense<-128> : tensor<i32>} : () -> tensor<i32>
// CHECK-DAG: %[[w0_scale:.*]] = "tf.Const"() {value = dense<0.00477493973> : tensor<f32>} : () -> tensor<f32>
// CHECK-DAG: %[[w_b_zp:.*]]  = "tf.Const"() {value = dense<0> : tensor<i32>} : () -> tensor<i32>
// CHECK-DAG: %[[b0_scale:.*]] = "tf.Const"() {value = dense<1.85075514E-5> : tensor<f32>} : () -> tensor<f32>
// CHECK-DAG: %[[mid_scale:.*]] = "tf.Const"() {value = dense<0.00141507247> : tensor<f32>} : () -> tensor<f32>
// CHECK-DAG: %[[w1_scale:.*]] = "tf.Const"() {value = dense<0.00477652298> : tensor<f32>} : () -> tensor<f32>
// CHECK-DAG: %[[b1_scale:.*]] = "tf.Const"() {value = dense<6.75912588E-6> : tensor<f32>} : () -> tensor<f32>
// CHECK-DAG: %[[out_scale:.*]] = "tf.Const"() {value = dense<7.24974147E-4> : tensor<f32>} : () -> tensor<f32>
// CHECK-DAG: %[[quantized:.*]] = "tf.PartitionedCall"(%arg0, %[[in_scale]], %[[in_out_zp]]) {config = "", config_proto = "", executor_type = "", f = @quantize_i8}
// CHECK-DAG: %[[conv0_dequantized:.*]] = "tf.PartitionedCall"(%[[quantized]], %[[w0_quantized]], %[[b0_quantized]], %[[in_scale]], %[[in_out_zp]], %[[w0_scale]], %[[w_b_zp]], %[[b0_scale]], %[[w_b_zp]], %[[mid_scale]], %[[in_out_zp]]) {config = "", config_proto = "", executor_type = "", f = @quantized_conv2d_with_bias_and_relu6_float_output_fn_1}
// CHECK-DAG: %[[conv0_quantized:.*]] = "tf.PartitionedCall"(%[[quantized]], %[[w0_quantized]], %[[b0_quantized]], %[[in_scale]], %[[in_out_zp]], %[[w0_scale]], %[[w_b_zp]], %[[b0_scale]], %[[w_b_zp]], %[[mid_scale]], %[[in_out_zp]]) {config = "", config_proto = "", executor_type = "", f = @quantized_conv2d_with_bias_and_relu6_fn_1}
// CHECK-DAG: %[[conv1_dequantized:.*]] = "tf.PartitionedCall"(%[[conv0_quantized]], %[[w1_quantized]], %[[b1_quantized]], %[[mid_scale]], %[[in_out_zp]], %[[w1_scale]], %[[w_b_zp]], %[[b1_scale]], %[[w_b_zp]], %[[out_scale]], %[[in_out_zp]]) {config = "", config_proto = "", executor_type = "", f = @quantized_conv2d_with_bias_and_relu6_float_output_fn_0}
// CHECK-DAG: %[[identity:.*]] = "tf.Identity"(%[[conv1_dequantized]])
// CHECK-DAG: %[[conv0_float:.*]] = "tf.PartitionedCall"(%arg0, %[[w0_float]], %[[b0_float]]) {config = "", config_proto = "", device = "", executor_type = "", f = @composite_conv2d_with_bias_and_relu6_fn_2_00}
// CHECK-DAG: %[[conv1_float:.*]] = "tf.PartitionedCall"(%[[conv0_dequantized]], %[[w1_float]], %[[b1_float]]) {config = "", config_proto = "", device = "", executor_type = "", f = @composite_conv2d_with_bias_and_relu6_fn_1_00}
// CHECK-DAG: "tf.DumpTensor"(%[[conv0_dequantized]]) {device = "", enabled = true, file_name = "quantized_tensor_data.pb", func_name = "conv_with_dump", log_dir_path = "/tmp/dumps/composite_conv2d_with_bias_and_relu6_fn_2", node_name = "Conv2D"}
// CHECK-DAG: "tf.DumpTensor"(%[[conv0_float]]) {device = "", enabled = true, file_name = "unquantized_tensor_data.pb", func_name = "conv_with_dump", log_dir_path = "/tmp/dumps/composite_conv2d_with_bias_and_relu6_fn_2", node_name = "Conv2D"}
// CHECK-DAG: "tf.DumpTensor"(%[[conv1_dequantized]]) {device = "", enabled = true, file_name = "quantized_tensor_data.pb", func_name = "conv_with_dump", log_dir_path = "/tmp/dumps/composite_conv2d_with_bias_and_relu6_fn_1", node_name = "Conv2D_1"}
// CHECK-DAG: "tf.DumpTensor"(%[[conv1_float]]) {device = "", enabled = true, file_name = "unquantized_tensor_data.pb", func_name = "conv_with_dump", log_dir_path = "/tmp/dumps/composite_conv2d_with_bias_and_relu6_fn_1", node_name = "Conv2D_1"}
// CHECK-DAG: return %[[identity]]
}
