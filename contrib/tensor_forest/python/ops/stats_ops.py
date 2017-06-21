# Copyright 2017 The TensorFlow Authors. All Rights Reserved.
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
"""Stats ops python wrappers."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.contrib.tensor_forest.python.ops import gen_stats_ops
# pylint: disable=unused-import
from tensorflow.contrib.tensor_forest.python.ops.gen_stats_ops import finalize_tree
from tensorflow.contrib.tensor_forest.python.ops.gen_stats_ops import grow_tree_v4
from tensorflow.contrib.tensor_forest.python.ops.gen_stats_ops import process_input_v4
# pylint: enable=unused-import

from tensorflow.contrib.util import loader
from tensorflow.python.framework import ops
from tensorflow.python.ops import resources
from tensorflow.python.platform import resource_loader
from tensorflow.python.training import saver


_stats_ops = loader.load_op_library(
    resource_loader.get_path_to_datafile("_stats_ops.so"))


ops.NotDifferentiable("FertileStatsVariable")
ops.NotDifferentiable("FertileStatsSerialize")
ops.NotDifferentiable("FertileStatsDeserialize")
ops.NotDifferentiable("GrowTreeV4")
ops.NotDifferentiable("ProcessInputV4")
ops.NotDifferentiable("FinalizeTree")


class FertileStatsVariableSavable(saver.BaseSaverBuilder.SaveableObject):
  """SaveableObject implementation for FertileStatsVariable."""

  def __init__(self, params, stats_handle, create_op, name):
    """Creates a FertileStatsVariableSavable object.

    Args:
      params: A TensorForestParams object.
      stats_handle: handle to the tree variable.
      create_op: the op to initialize the variable.
      name: the name to save the tree variable under.
    """
    self.params = params
    tensor = gen_stats_ops.fertile_stats_serialize(
        stats_handle, params=params.serialized_params_proto)
    # slice_spec is useful for saving a slice from a variable.
    # It's not meaningful the tree variable. So we just pass an empty value.
    slice_spec = ""
    specs = [saver.BaseSaverBuilder.SaveSpec(tensor, slice_spec, name),]
    super(FertileStatsVariableSavable,
          self).__init__(stats_handle, specs, name)
    self._stats_handle = stats_handle
    self._create_op = create_op

  def restore(self, restored_tensors, unused_restored_shapes):
    """Restores the associated tree from 'restored_tensors'.

    Args:
      restored_tensors: the tensors that were loaded from a checkpoint.
      unused_restored_shapes: the shapes this object should conform to after
        restore. Not meaningful for trees.

    Returns:
      The operation that restores the state of the tree variable.
    """
    with ops.control_dependencies([self._create_op]):
      return gen_stats_ops.fertile_stats_deserialize(
          self._stats_handle, restored_tensors[0],
          params=self.params.serialized_params_proto)


def fertile_stats_variable(params, stats_config, name,
                           container=None):
  r"""Creates a stats object and returns a handle to it.

  Args:
    params: A TensorForestParams object.
    stats_config: A `Tensor` of type `string`. Serialized proto of the stats.
    name: A name for the variable.
    container: An optional `string`. Defaults to `""`.

  Returns:
    A `Tensor` of type mutable `string`. The handle to the stats.
  """
  with ops.name_scope(name, "FertileStatsVariable") as name:
    resource_handle = gen_stats_ops.fertile_stats_resource_handle_op(
        container, name, name=name)

    create_op = gen_stats_ops.create_fertile_stats_variable(
        resource_handle, stats_config,
        params=params.serialized_params_proto)
    is_initialized_op = gen_stats_ops.fertile_stats_is_initialized_op(
        resource_handle)
    # Adds the variable to the savable list.
    saveable = FertileStatsVariableSavable(params, resource_handle, create_op,
                                           "stats_checkpoint_{0}".format(name))
    ops.add_to_collection(ops.GraphKeys.SAVEABLE_OBJECTS, saveable)
    resources.register_resource(resource_handle, create_op, is_initialized_op)
    return resource_handle
