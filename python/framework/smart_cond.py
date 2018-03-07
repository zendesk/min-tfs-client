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
"""smart_cond and related utilties."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python import pywrap_tensorflow as c_api
from tensorflow.python.framework import errors
from tensorflow.python.framework import ops
from tensorflow.python.framework import tensor_util
from tensorflow.python.ops import control_flow_ops


def smart_cond(pred, true_fn=None, false_fn=None, name=None):
  """Return either `true_fn()` if predicate `pred` is true else `false_fn()`.

  If `pred` is a bool or has a constant value, we return either `true_fn()`
  or `false_fn()`, otherwise we use `tf.cond` to dynamically route to both.

  Arguments:
    pred: A scalar determining whether to return the result of `true_fn` or
      `false_fn`.
    true_fn: The callable to be performed if pred is true.
    false_fn: The callable to be performed if pred is false.
    name: Optional name prefix when using `tf.cond`.

  Returns:
    Tensors returned by the call to either `true_fn` or `false_fn`.

  Raises:
    TypeError: If `true_fn` or `false_fn` is not callable.
  """
  if not callable(true_fn):
    raise TypeError("`true_fn` must be callable.")
  if not callable(false_fn):
    raise TypeError("`false_fn` must be callable.")

  pred_value = smart_constant_value(pred)
  if pred_value is not None:
    if pred_value:
      return true_fn()
    else:
      return false_fn()
  else:
    return control_flow_ops.cond(pred, true_fn=true_fn, false_fn=false_fn,
                                 name=name)


def smart_constant_value(pred):
  """Return the bool value for `pred`, or None if `pred` had a dynamic value.

  Arguments:
    pred: A scalar, either a Python bool or tensor.

  Returns:
    True or False if `pred` has a constant boolean value, None otherwise.

  Raises:
    TypeError: If `pred` is not a Tensor or bool.
  """
  if pred in {0, 1}:  # Accept 1/0 as valid boolean values
    pred_value = bool(pred)
  elif isinstance(pred, bool):
    pred_value = pred
  elif isinstance(pred, ops.Tensor):
    pred_value = tensor_util.constant_value(pred)
    # TODO(skyewm): consider folding this into tensor_util.constant_value when
    # _USE_C_API is removed (there may be performance and correctness bugs, so I
    # wanted to limit the change hidden behind _USE_C_API).
    # pylint: disable=protected-access
    if pred_value is None and ops._USE_C_API:
      with errors.raise_exception_on_not_ok_status() as status:
        pred_value = c_api.TF_TryEvaluateConstant_wrapper(
            pred.graph._c_graph, pred._as_tf_output(), status)
    # pylint: enable=protected-access

  else:
    raise TypeError("`pred` must be a Tensor, or a Python bool, or 1 or 0. "
                    "Found instead: %s" % pred)
  return pred_value
