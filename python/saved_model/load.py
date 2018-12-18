# Copyright 2018 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""Import a checkpointable object from a SavedModel."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os

from tensorflow.python.eager import function
from tensorflow.python.framework import function_def_to_graph as function_def_lib
from tensorflow.python.lib.io import file_io
from tensorflow.python.ops import init_ops
from tensorflow.python.ops import resource_variable_ops
from tensorflow.python.ops import variables
from tensorflow.python.saved_model import constants
from tensorflow.python.saved_model import function_deserialization
from tensorflow.python.saved_model import loader_impl
from tensorflow.python.saved_model import saved_object_graph_pb2
from tensorflow.python.saved_model import utils_impl as saved_model_utils
from tensorflow.python.training.checkpointable import tracking
from tensorflow.python.training.checkpointable import util
from tensorflow.python.util import compat


class _Loader(object):
  """Helper class to load an object-based SavedModel."""

  def __init__(self, object_graph_proto, saved_model_proto, export_dir):
    meta_graph = saved_model_proto.meta_graphs[0]
    self._asset_file_def = meta_graph.asset_file_def
    self._proto = object_graph_proto
    self._export_dir = export_dir
    self._load_func_graphs(meta_graph.graph_def.library)
    self._load_all()
    self._bind_function_captures()
    self._restore_checkpoint()

  def _load_func_graphs(self, function_library):
    # TODO(allenl): Do we need to do name mapping here? Not quite sure what
    # happens when loaded names collide with existing names.
    # TODO(andresp): Look into restoring nested and gradient functions in the
    # right order.
    self._functions = {}
    for fdef in function_library.function:
      graph = function_def_lib.function_def_to_graph(fdef)
      self._functions[fdef.signature.name] = function.Function(graph)

  def _bind_function_captures(self):
    """Setup captured tensors in restored concrete functions."""
    seen_functions = set()
    for object_proto in self._proto.nodes:
      if object_proto.WhichOneof("kind") == "function":
        for monomorphic_function in object_proto.function.monomorphic_function:
          name = monomorphic_function.concrete_function
          bound_inputs = [
              self._get_tensor_from_node(node_id)
              for node_id in monomorphic_function.bound_inputs]
          if name in seen_functions:
            if self._functions[name]._captured_inputs != bound_inputs:  # pylint: disable=protected-access
              raise NotImplementedError(
                  "Function %s is used more than once with different "
                  "captured inputs." % name)
          else:
            seen_functions.add(name)
            # TODO(andresp): This is only injecting the captured inputs into the
            # concrete function, note that we did not modify the FuncGraph
            # itself.
            self._functions[name]._captured_inputs = bound_inputs  # pylint: disable=protected-access

  def _get_tensor_from_node(self, node_id):
    obj = self._nodes[node_id]
    if resource_variable_ops.is_resource_variable(obj):
      return obj.handle
    elif isinstance(obj, tracking.TrackableAsset):
      return obj.asset_path.handle
    raise ValueError("Can't convert node %s to tensor" % (type(obj)))

  def _load_all(self):
    self._nodes = [self._recreate(proto) for proto in self._proto.nodes]
    # After creating the objects, construct the edges between the objects.
    for obj, object_proto in zip(self._nodes, self._proto.nodes):
      for reference in object_proto.children:
        setattr(obj, reference.local_name, self._nodes[reference.node_id])

  def _restore_checkpoint(self):
    variables_path = saved_model_utils.get_variables_path(self._export_dir)
    saver = util.CheckpointableSaver(self.get(0))
    saver.restore(variables_path).assert_consumed()

  def get(self, node_id):
    return self._nodes[node_id]

  def _recreate(self, proto):
    """Creates a Python object from a SavedObject protocol buffer."""
    factory = {
        "user_object": lambda: self._recreate_user_object(proto.user_object),
        "asset": lambda: self._recreate_asset(proto.asset),
        "function": lambda: self._recreate_function(proto.function),
        "variable": lambda: self._recreate_variable(proto.variable),
    }
    kind = proto.WhichOneof("kind")
    if kind not in factory:
      raise ValueError("Unknown SavedObject type: %r" % kind)
    return factory[kind]()

  def _recreate_user_object(self, proto):
    del proto
    return tracking.Checkpointable()

  def _recreate_asset(self, proto):
    filename = os.path.join(
        saved_model_utils.get_assets_dir(self._export_dir),
        self._asset_file_def[proto.asset_file_def_index].filename)
    return tracking.TrackableAsset(filename)

  def _recreate_function(self, proto):
    return function_deserialization.recreate_polymorphic_function(
        proto, self._functions)

  def _recreate_variable(self, proto):
    # TODO(andresp): Can we use the checkpointed value as initializer?
    dummy_value = init_ops.Zeros(dtype=proto.dtype)(shape=proto.shape)
    return variables.Variable(dummy_value)


def _load_saved_object_graph_proto(filename):
  with file_io.FileIO(filename, "rb") as f:
    contents = f.read()
    return saved_object_graph_pb2.SavedObjectGraph.FromString(contents)


def load(export_dir):
  """Load a SavedModel from `export_dir`."""
  saved_model_proto = loader_impl.parse_saved_model(export_dir)
  object_graph_filename = os.path.join(
      compat.as_bytes(export_dir),
      compat.as_bytes(constants.EXTRA_ASSETS_DIRECTORY),
      compat.as_bytes("object_graph.pb"))
  if file_io.file_exists(object_graph_filename):
    object_graph_proto = _load_saved_object_graph_proto(object_graph_filename)
    loader = _Loader(object_graph_proto,
                     saved_model_proto,
                     export_dir)
    root = loader.get(0)
  else:
    raise NotImplementedError(
        "Currently only SavedModels exported with `tf.saved_model.save` may be "
        "imported. Other SavedModels may eventually be supported via load().")
  return root
