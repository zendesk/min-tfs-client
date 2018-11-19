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
"""Momentum for TensorFlow."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.framework import ops
from tensorflow.python.keras.optimizer_v2 import optimizer_v2
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import resource_variable_ops
from tensorflow.python.training import training_ops


class SGD(optimizer_v2.OptimizerV2):
  """Stochastic gradient descent and momentum optimizer.

  Computes:
  ```
  theta(t+1) = theta(t) - learning_rate * gradient
  gradient is evaluated at theta(t).
  ```

  or Computes (if `use_nesterov = False`):
  ```
  v(t+1) = momentum * v(t) - learning_rate * gradient
  theta(t+1) = theta(t) + v(t+1)
  if `nesterov` is False, gradient is evaluated at theta(t).
  if `nesterov` is True, gradient is evaluated at theta(t) + momentum * v(t),
    and the variables always store theta + m v instead of theta
  ```

  Some of the args below are hyperparameters, where a hyperparameter is
  defined as a scalar Tensor, a regular Python value, or a callable (which
  will be evaluated when `apply_gradients` is called) returning a scalar
  Tensor or a Python value.

  @compatibility(eager)
  When eager execution is enabled, learning_rate can be a callable that takes
  no arguments and returns the actual value to use. This can be useful for
  changing these values across different invocations of optimizer functions.
  @end_compatibility

  # References
      nesterov = True, See [Sutskever et al., 2013](
        http://jmlr.org/proceedings/papers/v28/sutskever13.pdf).
  """

  def __init__(self,
               learning_rate=0.001,
               momentum=0.0,
               nesterov=False,
               name="SGD"):
    """Construct a new Stochastic Gradient Descent or Momentum optimizer.

    Arguments:
      learning_rate: float hyperparameter >= 0. Learning rate.
      momentum: float hyperparameter >= 0 that accelerates SGD in the relevant
        direction and dampens oscillations.
      nesterov: boolean. Whether to apply Nesterov momentum.
      name: Optional name prefix for the operations created when applying
        gradients.  Defaults to 'SGD'.
    """
    super(SGD, self).__init__(name)
    self._set_hyper("learning_rate", learning_rate)

    self._momentum = False
    if isinstance(momentum, ops.Tensor) or callable(momentum) or momentum > 0:
      self._momentum = True
    if isinstance(momentum, (int, float)) and (momentum < 0 or momentum > 1):
      raise ValueError("`momentum` must be between [0, 1].")
    self._set_hyper("momentum", momentum)

    self._nesterov = nesterov

  def _create_slots(self, var_list):
    if self._momentum:
      for var in var_list:
        self.add_slot(var, "momentum")

  def _resource_apply_dense(self, grad, var):
    learning_rate = self._get_hyper("learning_rate")
    if self._momentum:
      momentum_var = self.get_slot(var, "momentum")
      return training_ops.resource_apply_momentum(
          var.handle,
          momentum_var.handle,
          math_ops.cast(learning_rate, grad.dtype.base_dtype),
          grad,
          math_ops.cast(self._get_hyper("momentum"), grad.dtype.base_dtype),
          use_locking=self._use_locking,
          use_nesterov=self._nesterov)
    else:
      return training_ops.resource_apply_gradient_descent(
          var.handle,
          math_ops.cast(learning_rate, grad.dtype.base_dtype),
          grad,
          use_locking=self._use_locking)

  def _resource_apply_sparse_duplicate_indices(self, grad, var, indices):
    if self._momentum:
      return super(SGD, self)._resource_apply_sparse_duplicate_indices(
          grad, var, indices)
    else:
      return resource_variable_ops.resource_scatter_add(
          var.handle, indices, -grad * math_ops.cast(
              self._get_hyper("learning_rate"), grad.dtype.base_dtype))

  def _resource_apply_sparse(self, grad, var, indices):
    # This method is only needed for momentum optimization.
    learning_rate = self._get_hyper("learning_rate")
    momentum_var = self.get_slot(var, "momentum")
    return training_ops.resource_sparse_apply_momentum(
        var.handle,
        momentum_var.handle,
        math_ops.cast(learning_rate, grad.dtype.base_dtype),
        grad,
        indices,
        math_ops.cast(self._get_hyper("momentum"), grad.dtype.base_dtype),
        use_locking=self._use_locking,
        use_nesterov=self._nesterov)

  def get_config(self):
    config = super(SGD, self).get_config()
    config.update({
        "learning_rate": self._serialize_hyperparameter("learning_rate"),
        "momentum": self._serialize_hyperparameter("momentum"),
        "nesterov": self._nesterov,
    })
    return config
