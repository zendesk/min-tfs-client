# Copyright 2016 The TensorFlow Authors. All Rights Reserved.
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

"""`Evaluable` interface."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import abc


class Evaluable(object):
  """Interface for objects that are evaluatable by, e.g., `Experiment`.
  """
  __metaclass__ = abc.ABCMeta

  @abc.abstractmethod
  def evaluate(
      self, x=None, y=None, input_fn=None, feed_fn=None, batch_size=None,
      steps=None, metrics=None, name=None):
    """Evaluates given model with provided evaluation data.

    Evaluates on the given input data. If `input_fn` is provided, that
    input function should raise an end-of-input exception (`OutOfRangeError` or
    `StopIteration`) after one epoch of the training data has been provided.

    By default, the whole evaluation dataset is used. If `steps` is provided,
    only `steps` batches of size `batch_size` are processed.

    The return value is a dict containing the metrics specified in `metrics`, as
    well as an entry `global_step` which contains the value of the global step
    for which this evaluation was performed.

    Args:
      x: Matrix of shape [n_samples, n_features...]. Can be iterator that
         returns arrays of features. The training input samples for fitting the
         model. If set, `input_fn` must be `None`.
      y: Vector or matrix [n_samples] or [n_samples, n_outputs]. Can be
         iterator that returns array of targets. The training target values
         (class labels in classification, real numbers in regression). If set,
         `input_fn` must be `None`.
      input_fn: Input function. If set, `x`, `y`, and `batch_size` must be
        `None`.
      feed_fn: Function creating a feed dict every time it is called. Called
        once per iteration. Must be `None` if `input_fn` is provided.
      batch_size: minibatch size to use on the input, defaults to first
        dimension of `x`, if specified. Must be `None` if `input_fn` is
        provided.
      steps: Number of steps for which to evaluate model. If `None`, evaluate
        until running tensors generated by `metrics` raises an exception.
      metrics: Dict of metric ops to run. If `None`, the default metric
        functions are used; if `{}`, no metrics are used. If model has one
        output (i.e., returning single predction), keys are `str`, e.g.
        `'accuracy'` - just a name of the metric that will show up in
        the logs / summaries. Otherwise, keys are tuple of two `str`, e.g.
        `('accuracy', 'classes')`- name of the metric and name of `Tensor` in
        the predictions to run this metric on.

        Metric ops should support streaming, e.g., returning
        update_op and value tensors. See more details in
        ../../../metrics/python/metrics/ops/streaming_metrics.py.
      name: Name of the evaluation if user needs to run multiple evaluations on
        different data sets, such as on training data vs test data.

    Returns:
      Returns `dict` with evaluation results.
    """
    raise NotImplementedError
