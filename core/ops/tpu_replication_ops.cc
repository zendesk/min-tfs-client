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

#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/shape_inference.h"

namespace tensorflow {

using shape_inference::InferenceContext;
using shape_inference::ShapeHandle;

REGISTER_OP("TPUReplicateMetadata")
    .Attr("num_replicas: int >= 0")
    .Attr("num_cores_per_replica: int = 1")
    .Attr("topology: string = \"\"")
    .Attr("use_tpu: bool = true")
    .Attr("device_assignment: list(int) = []")
    // Deprecated. Use num_cores_per_replica instead.
    .Attr("computation_shape: list(int) = []")
    .Attr("host_compute_core: list(string) = []")
    .Attr("padding_map: list(string) = []")
    .Attr("step_marker_location: string = \"STEP_MARK_AT_ENTRY\"")
    .SetShapeFn(shape_inference::UnknownShape);

REGISTER_OP("TPUReplicatedInput")
    .Input("inputs: N * T")
    .Output("output: T")
    .Attr("N: int >= 1")
    .Attr("T: type")
    .SetShapeFn([](InferenceContext* c) {
      ShapeHandle cur = c->input(c->num_inputs() - 1);
      for (int i = c->num_inputs() - 2; i >= 0; --i) {
        TF_RETURN_WITH_CONTEXT_IF_ERROR(c->Merge(c->input(i), cur, &cur),
                                        "From merging shape ", i,
                                        " with other shapes.");
      }
      c->set_output(0, cur);

      // If this is a resource, unify the resource shapes.
      DataType dtype;
      TF_RETURN_IF_ERROR(c->GetAttr("T", &dtype));
      if (dtype == DT_RESOURCE) {
        const std::vector<shape_inference::ShapeAndType>* shapes_and_types =
            nullptr;
        for (int i = c->num_inputs() - 1; i >= 0; --i) {
          if (shapes_and_types) {
            // The return value of MergeInputHandleShapesAndTypes indicates
            // the shape was refined, not that there was an error.
            // TODO(phawkins): there seems to be no way to discover errors.
            (void)c->MergeInputHandleShapesAndTypes(i, *shapes_and_types);
          } else {
            shapes_and_types = c->input_handle_shapes_and_types(i);
          }
        }
        if (shapes_and_types) {
          c->set_output_handle_shapes_and_types(0, *shapes_and_types);
        }
      }
      return Status::OK();
    });

REGISTER_OP("TPUReplicatedOutput")
    .Input("input: T")
    .Output("outputs: num_replicas * T")
    .Attr("num_replicas: int >= 1")
    .Attr("T: type")
    .SetShapeFn([](InferenceContext* c) {
      for (int i = 0; i < c->num_outputs(); ++i) {
        c->set_output(i, c->input(0));
      }
      return Status::OK();
    });

REGISTER_OP("TPUCompilationResult")
    .Output("output: string")
    .SetShapeFn(shape_inference::ScalarShape);

REGISTER_OP("_TPUReplicate")
    .Attr("computation: func")
    .Attr("num_replicas: int >= 1")
    .Attr("num_cores_per_replica: int = 1")
    .Attr("topology: string = \"\"")
    .Attr("use_tpu: bool = true")
    .Attr("device_assignment: list(int) = []")
    .Attr("host_compute_core: list(string) = []")
    .Attr("Tinputs: list(type) >= 0")
    .Attr("Tbroadcast_inputs: list(type) >= 0")
    .Attr("NumVariables: int >= 0")
    .Attr("Tguaranteed_constants: list(type) >= 0")
    .Attr("output_types: list(type) >= 0")
    .Attr("padding_map: list(string) = []")
    .Attr("step_marker_location: string = \"STEP_MARK_AT_ENTRY\"")
    .Input("inputs: Tinputs")
    .Input("broadcast_inputs: Tbroadcast_inputs")
    .Input("variables: NumVariables * resource")
    .Input("guaranteed_constants: Tguaranteed_constants")
    .Output("outputs: output_types")
    .SetShapeFn(shape_inference::UnknownShape);

}  // namespace tensorflow
