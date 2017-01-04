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
"""Implementation of k-means clustering on top of tf.learn API."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np

from tensorflow.contrib.factorization.python.ops import clustering_ops
from tensorflow.contrib.framework.python.ops import variables
from tensorflow.contrib.learn.python.learn import evaluable
from tensorflow.contrib.learn.python.learn import trainable
from tensorflow.contrib.learn.python.learn.estimators import estimator
from tensorflow.contrib.learn.python.learn.estimators.model_fn import ModelFnOps
from tensorflow.python.framework import ops
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import logging_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import state_ops
from tensorflow.python.ops.control_flow_ops import with_dependencies
from tensorflow.python.training import session_run_hook
from tensorflow.python.training.session_run_hook import SessionRunArgs

SQUARED_EUCLIDEAN_DISTANCE = clustering_ops.SQUARED_EUCLIDEAN_DISTANCE
COSINE_DISTANCE = clustering_ops.COSINE_DISTANCE
RANDOM_INIT = clustering_ops.RANDOM_INIT
KMEANS_PLUS_PLUS_INIT = clustering_ops.KMEANS_PLUS_PLUS_INIT


# TODO(agarwal,ands): support sharded input.
class KMeansClustering(evaluable.Evaluable, trainable.Trainable):
  """An Estimator fo rK-Means clustering."""
  SCORES = 'scores'
  CLUSTER_IDX = 'cluster_idx'
  CLUSTERS = 'clusters'
  ALL_SCORES = 'all_scores'
  LOSS_OP_NAME = 'kmeans_loss'

  def __init__(self,
               num_clusters,
               model_dir=None,
               initial_clusters=clustering_ops.RANDOM_INIT,
               distance_metric=clustering_ops.SQUARED_EUCLIDEAN_DISTANCE,
               random_seed=0,
               use_mini_batch=True,
               kmeans_plus_plus_num_retries=2,
               config=None):
    """Creates a model for running KMeans training and inference.

    Args:
      num_clusters: number of clusters to train.
      model_dir: the directory to save the model results and log files.
      initial_clusters: specifies how to initialize the clusters for training.
        See clustering_ops.kmeans for the possible values.
      distance_metric: the distance metric used for clustering.
        See clustering_ops.kmeans for the possible values.
      random_seed: Python integer. Seed for PRNG used to initialize centers.
      use_mini_batch: If true, use the mini-batch k-means algorithm. Else assume
        full batch.
      kmeans_plus_plus_num_retries: For each point that is sampled during
        kmeans++ initialization, this parameter specifies the number of
        additional points to draw from the current distribution before selecting
        the best. If a negative value is specified, a heuristic is used to
        sample O(log(num_to_sample)) additional points.
      config: See Estimator
    """
    self._num_clusters = num_clusters
    self._training_initial_clusters = initial_clusters
    self._distance_metric = distance_metric
    self._random_seed = random_seed
    self._use_mini_batch = use_mini_batch
    self._kmeans_plus_plus_num_retries = kmeans_plus_plus_num_retries
    self._estimator = estimator.Estimator(
        model_fn=self._get_model_function(), model_dir=model_dir)

  class LossRelativeChangeHook(session_run_hook.SessionRunHook):
    """Stops when the change in loss goes below a tolerance."""

    def __init__(self, tolerance):
      """Initializes LossRelativeChangeHook.

      Args:
        tolerance: A relative tolerance of change between iterations.
      """
      self._tolerance = tolerance
      self._prev_loss = None

    def begin(self):
      self._loss_tensor = ops.get_default_graph().get_tensor_by_name(
          KMeansClustering.LOSS_OP_NAME + ':0')
      assert self._loss_tensor is not None

    def before_run(self, run_context):
      del run_context
      return SessionRunArgs(
          fetches={KMeansClustering.LOSS_OP_NAME: self._loss_tensor})

    def after_run(self, run_context, run_values):
      loss = run_values.results[KMeansClustering.LOSS_OP_NAME]
      assert loss is not None
      if self._prev_loss is not None:
        relative_change = (abs(loss - self._prev_loss) /
                           (1 + abs(self._prev_loss)))
        if relative_change < self._tolerance:
          run_context.request_stop()
      self._prev_loss = loss

  @property
  def model_dir(self):
    """See Evaluable."""
    return self._estimator.model_dir

  def fit(self,
          input_fn=None,
          steps=None,
          monitors=None,
          max_steps=None,
          relative_tolerance=None):
    """Trains a k-means clustering on x.

    Note: See Estimator for logic for continuous training and graph
      construction across multiple calls to fit.

    Args:
      input_fn: see Trainable.fit.
      steps: see Trainable.fit.
      monitors: see Trainable.fit.
      max_steps: see Trainable.fit.
      relative_tolerance: A relative tolerance of change in the loss between
        iterations.  Stops learning if the loss changes less than this amount.
        Note that this may not work correctly if use_mini_batch=True.

    Returns:
      Returns self.
    """
    if relative_tolerance is not None:
      if monitors is None:
        monitors = []
      monitors.append(self.LossRelativeChangeHook(relative_tolerance))
    # Make sure that we will eventually terminate.
    assert ((monitors is not None and len(monitors)) or (steps is not None) or
            (max_steps is not None))
    self._estimator.fit(input_fn=input_fn,
                        steps=steps,
                        max_steps=max_steps,
                        monitors=monitors)
    return self

  def evaluate(self,
               input_fn=None,
               feed_fn=None,
               steps=None,
               metrics=None,
               name=None,
               checkpoint_path=None,
               hooks=None):
    """See Evaluable.evaluate."""
    return self._estimator.evaluate(
        input_fn=input_fn,
        feed_fn=feed_fn,
        steps=steps,
        metrics=metrics,
        name=name,
        checkpoint_path=checkpoint_path,
        hooks=hooks)

  def predict(self, input_fn=None, outputs=None, as_iterable=False):
    """See BaseEstimator.predict."""

    outputs = outputs or [KMeansClustering.CLUSTER_IDX]
    assert isinstance(outputs, list)
    results = self._estimator.predict(
        input_fn=input_fn, outputs=outputs, as_iterable=as_iterable)
    if len(outputs) == 1 and not as_iterable:
      return results[outputs[0]]
    else:
      return results

  def score(self, input_fn=None, steps=None):
    """Predict total sum of distances to nearest clusters.

    Note that this function is different from the corresponding one in sklearn
    which returns the negative of the sum of distances.

    Args:
      input_fn: see predict.
      steps: see predict.

    Returns:
      Total sum of distances to nearest clusters.
    """
    return np.sum(
        self.evaluate(
            input_fn=input_fn, steps=steps)[KMeansClustering.SCORES])

  def transform(self, input_fn=None, as_iterable=False):
    """Transforms each element to distances to cluster centers.

    Note that this function is different from the corresponding one in sklearn.
    For SQUARED_EUCLIDEAN distance metric, sklearn transform returns the
    EUCLIDEAN distance, while this function returns the SQUARED_EUCLIDEAN
    distance.

    Args:
      input_fn: see predict.
      as_iterable: see predict

    Returns:
      Array with same number of rows as x, and num_clusters columns, containing
      distances to the cluster centers.
    """
    return self.predict(
        input_fn=input_fn,
        outputs=[KMeansClustering.ALL_SCORES],
        as_iterable=as_iterable)

  def clusters(self):
    """Returns cluster centers."""
    return self._estimator.get_variable_value(self.CLUSTERS)

  def _parse_tensor_or_dict(self, features):
    if isinstance(features, dict):
      keys = sorted(features.keys())
      with ops.colocate_with(features[keys[0]]):
        features = array_ops.concat(1, [features[k] for k in keys])
    return features

  def _get_model_function(self):
    """Creates a model function."""

    def _model_fn(features, labels, mode):
      """Model function."""
      assert labels is None, labels
      (all_scores, model_predictions, losses,
       training_op) = clustering_ops.KMeans(
           self._parse_tensor_or_dict(features),
           self._num_clusters,
           self._training_initial_clusters,
           self._distance_metric,
           self._use_mini_batch,
           random_seed=self._random_seed,
           kmeans_plus_plus_num_retries=self.
           _kmeans_plus_plus_num_retries).training_graph()
      incr_step = state_ops.assign_add(variables.get_global_step(), 1)
      loss = math_ops.reduce_sum(losses, name=KMeansClustering.LOSS_OP_NAME)
      logging_ops.scalar_summary('loss/raw', loss)
      training_op = with_dependencies([training_op, incr_step], loss)
      predictions = {
          KMeansClustering.ALL_SCORES: all_scores[0],
          KMeansClustering.CLUSTER_IDX: model_predictions[0],
      }
      eval_metric_ops = {KMeansClustering.SCORES: loss,}
      return ModelFnOps(
          mode=mode,
          predictions=predictions,
          eval_metric_ops=eval_metric_ops,
          loss=loss,
          train_op=training_op)

    return _model_fn
