# Copyright 2015 The TensorFlow Authors. All Rights Reserved.
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

"""Standard functions for creating slots.

A slot is a `Variable` created with the same shape as a primary variable or
`Tensor`. A slot is always scoped in the namespace of the primary object and
typically has the same device and type.

Slots are typically used as accumulators to track values associated with
the primary object:

```python
# Optimizers can create a slot for each variable to track accumulators
accumulators = {var : create_zeros_slot(var, "momentum") for var in vs}
for var in vs:
  apply_momentum(var, accumulators[var], lr, grad, momentum_tensor)

# Slots can also be used for moving averages
mavg = create_slot(var, var.initialized_value(), "exponential_moving_avg")
update_mavg = mavg.assign_sub((mavg - var) * (1 - decay))
```
"""
# pylint: disable=g-bad-name

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.eager import context
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import init_ops
from tensorflow.python.ops import resource_variable_ops
from tensorflow.python.ops import variable_scope
from tensorflow.python.ops import variables
from tensorflow.python.training import distribute as distribute_lib


def _create_slot_var(primary, val, scope, validate_shape, shape, dtype):
  """Helper function for creating a slot variable."""

  # TODO(lukaszkaiser): Consider allowing partitioners to be set in the current
  # scope.
  current_partitioner = variable_scope.get_variable_scope().partitioner
  variable_scope.get_variable_scope().set_partitioner(None)
  # When init from val instead of callable initializer, the shape is expected to
  # be None, not <unknown> or any fully defined shape.
  shape = shape if callable(val) else None
  slot = variable_scope.get_variable(
      scope, initializer=val, trainable=False,
      use_resource=resource_variable_ops.is_resource_variable(primary),
      shape=shape, dtype=dtype,
      validate_shape=validate_shape)
  variable_scope.get_variable_scope().set_partitioner(current_partitioner)

  # pylint: disable=protected-access
  if isinstance(primary, variables.Variable) and primary._save_slice_info:
    # Primary is a partitioned variable, so we need to also indicate that
    # the slot is a partitioned variable.  Slots have the same partitioning
    # as their primaries.
    # For examples when using AdamOptimizer in linear model, slot.name
    # here can be "linear//weights/Adam:0", while primary.op.name is
    # "linear//weight". We want to get 'Adam' as real_slot_name, so we
    # remove "'linear//weight' + '/'" and ':0'.
    real_slot_name = slot.name[len(primary.op.name + "/"):-2]
    slice_info = primary._save_slice_info
    slot._set_save_slice_info(variables.Variable.SaveSliceInfo(
        slice_info.full_name + "/" + real_slot_name,
        slice_info.full_shape[:],
        slice_info.var_offset[:],
        slice_info.var_shape[:]))
  # pylint: enable=protected-access
  return slot


def create_slot(primary, val, name, colocate_with_primary=True):
  """Create a slot initialized to the given value.

  The type of the slot is determined by the given value.

  Args:
    primary: The primary `Variable` or `Tensor`.
    val: A `Tensor` specifying the initial value of the slot.
    name: Name to use for the slot variable.
    colocate_with_primary: Boolean.  If True the slot is located
      on the same device as `primary`.

  Returns:
    A `Variable` object.
  """
  # Scope the slot name in the namespace of the primary variable.
  # Set "primary.op.name + '/' + name" as default name, so the scope name of
  # optimizer can be shared when reuse is True. Meanwhile when reuse is False
  # and the same name has been previously used, the scope name will add '_N'
  # as suffix for unique identifications.
  validate_shape = val.get_shape().is_fully_defined()
  if context.executing_eagerly():
    prefix = primary._shared_name  # pylint: disable=protected-access
  else:
    prefix = primary.op.name
  with variable_scope.variable_scope(None, prefix + "/" + name):
    if colocate_with_primary:
      distribution_strategy = distribute_lib.get_distribution_strategy()
      with distribution_strategy.colocate_vars_with(primary):
        return _create_slot_var(primary, val, "", validate_shape, None, None)
    else:
      return _create_slot_var(primary, val, "", validate_shape, None, None)


def create_slot_with_initializer(primary, initializer, shape, dtype, name,
                                 colocate_with_primary=True):
  """Creates a slot initialized using an `Initializer`.

  The type of the slot is determined by the given value.

  Args:
    primary: The primary `Variable` or `Tensor`.
    initializer: An `Initializer`.  The initial value of the slot.
    shape: Shape of the initial value of the slot.
    dtype: Type of the value of the slot.
    name: Name to use for the slot variable.
    colocate_with_primary: Boolean.  If True the slot is located
      on the same device as `primary`.

  Returns:
    A `Variable` object.
  """
  # Scope the slot name in the namespace of the primary variable.
  # Set "primary.op.name + '/' + name" as default name, so the scope name of
  # optimizer can be shared when reuse is True. Meanwhile when reuse is False
  # and the same name has been previously used, the scope name will add '_N'
  # as suffix for unique identifications.
  validate_shape = shape.is_fully_defined()
  if context.executing_eagerly():
    prefix = primary._shared_name  # pylint: disable=protected-access
  else:
    prefix = primary.op.name
  with variable_scope.variable_scope(None, prefix + "/" + name):
    if colocate_with_primary:
      distribution_strategy = distribute_lib.get_distribution_strategy()
      with distribution_strategy.colocate_vars_with(primary):
        return _create_slot_var(primary, initializer, "", validate_shape, shape,
                                dtype)
    else:
      return _create_slot_var(primary, initializer, "", validate_shape, shape,
                              dtype)


def create_zeros_slot(primary, name, dtype=None, colocate_with_primary=True):
  """Create a slot initialized to 0 with same shape as the primary object.

  Args:
    primary: The primary `Variable` or `Tensor`.
    name: Name to use for the slot variable.
    dtype: Type of the slot variable.  Defaults to the type of `primary`.
    colocate_with_primary: Boolean.  If True the slot is located
      on the same device as `primary`.

  Returns:
    A `Variable` object.
  """
  if dtype is None:
    dtype = primary.dtype
  slot_shape = primary.get_shape()
  if slot_shape.is_fully_defined():
    initializer = init_ops.zeros_initializer(dtype)
    return create_slot_with_initializer(
        primary, initializer, slot_shape, dtype, name,
        colocate_with_primary=colocate_with_primary)
  else:
    if isinstance(primary, variables.Variable):
      slot_shape = array_ops.shape(primary.initialized_value())
    else:
      slot_shape = array_ops.shape(primary)
    val = array_ops.zeros(slot_shape, dtype=dtype)
    return create_slot(primary, val, name,
                       colocate_with_primary=colocate_with_primary)
