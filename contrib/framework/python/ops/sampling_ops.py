# Copyright 2016 Google Inc. All Rights Reserved.
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

"""Sampling functions."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np
from tensorflow.python.framework import constant_op
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import ops
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import check_ops
from tensorflow.python.ops import control_flow_ops
from tensorflow.python.ops import data_flow_ops
from tensorflow.python.ops import logging_ops
from tensorflow.python.ops import math_ops
from tensorflow.python.ops import random_ops
from tensorflow.python.platform import tf_logging as logging
from tensorflow.python.training import input as input_ops
from tensorflow.python.training import queue_runner

__all__ = ['stratified_sample',
           'stratified_sample_unknown_dist',]


# TODO(joelshor): Use an exponential-moving-average to estimate the initial
# class distribution and remove the requirement that it be provided.
def stratified_sample(data, labels, init_probs, target_probs, batch_size,
                      enqueue_many=False, queue_capacity=16,
                      threads_per_queue=1, name=None):
  """Stochastically creates batches based on per-class probabilities.

  This method discards examples. Internally, it creates one queue to amortize
  the cost of disk reads, and one queue to hold the properly-proportioned
  batch. See `stratified_sample_unknown_dist` for a function that performs
  stratified sampling with one queue per class and doesn't require knowing the
  class data-distribution ahead of time.

  Args:
    data: Tensor for data. Either one item or a batch, according to
        enqueue_many.
    labels: Tensor for label of data. Label is a single integer or a batch,
        depending on enqueue_many. It is not a one-hot vector.
    init_probs: 1D numpy or python array of class proportions in the data.
    target_probs: 1D numpy or python array of target class proportions in batch.
    batch_size: Size of batch to be returned.
    enqueue_many: Bool. If true, interpret input tensors as having a batch
        dimension.
    queue_capacity: Capacity of the large queue that holds input examples.
    threads_per_queue: Number of threads for the large queue that holds input
        examples and for the final queue with the proper class proportions.
    name: Optional prefix for ops created by this function.
  Raises:
    ValueError: enqueue_many is True and labels doesn't have a batch
        dimension, or if enqueue_many is False and labels isn't a scalar.
    ValueError: enqueue_many is True, and batch dimension on data and labels
        don't match.
    ValueError: if probs don't sum to one.
    ValueError: if a zero initial probability class has a nonzero target
        probability.
    TFAssertion: if labels aren't integers in [0, num classes).
  Returns:
    (data_batch, label_batch)

  Example:
    # Get tensor for a single data and label example.
    data, label = data_provider.Get(['data', 'label'])

    # Get stratified batch according to per-class probabilities.
    init_probs = [1.0/NUM_CLASSES for _ in range(NUM_CLASSES)]
    target_probs = [...distribution you want...]
    data_batch, labels = tf.contrib.framework.sampling_ops.stratified_sample(
        data, label, init_probs, target_probs)

    # Run batch through network.
    ...
  """
  with ops.op_scope([data, labels], name, 'stratified_sample'):
    data = ops.convert_to_tensor(data)
    labels = ops.convert_to_tensor(labels)
    # Reduce the case of a single example to that of a batch of size 1.
    if not enqueue_many:
      data = array_ops.expand_dims(data, 0)
      labels = array_ops.expand_dims(labels, 0)

    # Validate that input is consistent.
    data, labels, [init_probs, target_probs] = _verify_input(
        data, labels, [init_probs, target_probs])

    # Check that all zero initial probabilities also have zero target
    # probabilities.
    if np.any(np.logical_and(np.array(init_probs) == 0,
                             np.array(target_probs) != 0)):
      raise ValueError('Some initial probability class has nonzero target '
                       'probability.')

    # Calculate rejection sampling probabilities.
    reject_probs = _calculate_rejection_probabilities(init_probs, target_probs)
    proportion_rejected = np.sum(np.array(reject_probs) * np.array(init_probs))
    if proportion_rejected > .5:
      logging.warning('Proportion of examples rejected by sampler is high: ',
                      proportion_rejected)

    # Make a single queue to hold input examples.
    val, label = input_ops.batch([data, labels],
                                 batch_size=1,
                                 num_threads=threads_per_queue,
                                 capacity=queue_capacity,
                                 enqueue_many=True)
    val = array_ops.reshape(val, data.get_shape().with_rank_at_least(1)[1:])
    label = array_ops.reshape(
        label, labels.get_shape().with_rank_at_least(1)[1:])

    # Set up second queue containing batches that have the desired class
    # proportions.
    return _get_stratified_batch_from_tensors(
        val, label, reject_probs, batch_size, threads_per_queue)


def stratified_sample_unknown_dist(data, labels, probs, batch_size,
                                   enqueue_many=False, queue_capacity=16,
                                   threads_per_queue=1, name=None):
  """Stochastically creates batches based on per-class probabilities.

  **NOTICE** This sampler can be significantly slower than `stratified_sample`
  due to each thread discarding all examples not in its assigned class.

  This uses a number of threads proportional
  to the number of classes. See `stratified_sample` for an implementation that
  discards fewer examples and uses a fixed number of threads. This function's
  only advantage to `stratified_sample` is that the class data-distribution
  doesn't need to be known ahead of time.

  Args:
    data: Tensor for data. Either one item or a batch, according to
        enqueue_many.
    labels: Tensor for label of data. Label is a single integer or a batch,
        depending on enqueue_many. It is not a one-hot vector.
    probs: 1D numpy or python array of probabilities.
    batch_size: Size of batch to be returned.
    enqueue_many: Bool. If true, interpret input tensors as having a batch
        dimension.
    queue_capacity: Capacity of each per-class queue.
    threads_per_queue: Number of threads for each per-class queue.
    name: Optional prefix for ops created by this function.
  Raises:
    ValueError: enqueue_many is True and labels doesn't have a batch
        dimension, or if enqueue_many is False and labels isn't a scalar.
    ValueError: enqueue_many is True, and batch dimension of data and labels
        don't match.
    ValueError: if probs don't sum to one.
    TFAssertion: if labels aren't integers in [0, num classes).
  Returns:
    (data_batch, label_batch)

  Example:
    # Get tensor for a single data and label example.
    data, label = data_provider.Get(['data', 'label'])

    # Get stratified batch according to per-class probabilities.
    init_probs = [1.0/NUM_CLASSES for _ in range(NUM_CLASSES)]
    data_batch, labels = (
        tf.contrib.framework.sampling_ops.stratified_sample_unknown_dist(
            data, label, init_probs, 16))

    # Run batch through network.
    ...
  """
  with ops.op_scope([data, labels], name, 'stratified_sample_unknown_dist'):
    data = ops.convert_to_tensor(data)
    labels = ops.convert_to_tensor(labels)
    # Reduce the case of a single example to that of a batch of size 1.
    if not enqueue_many:
      data = array_ops.expand_dims(data, 0)
      labels = array_ops.expand_dims(labels, 0)

    # Validate that input is consistent.
    data, labels, [probs] = _verify_input(data, labels, [probs])

    # Make per-class queues.
    per_class_queues = _make_per_class_queues(
        data, labels, probs.size, queue_capacity, threads_per_queue)

    # Use the per-class queues to generate stratified batches.
    return _get_batch_from_per_class_queues(per_class_queues, probs, batch_size)


def _verify_input(data, labels, probs_list):
  """Verify that batched inputs are well-formed."""
  checked_probs_list = []
  for probs in probs_list:
    # Probabilities must be able to be converted to non-object numpy array.
    np_probs = np.asarray(probs)
    if np_probs.dtype == np.dtype('object'):
      raise ValueError('Probabilities must be able to be converted to a numpy '
                       'array.')
    checked_probs_list.append(np_probs)

    # Probabilities must sum to one.
    # TODO(joelshor): Investigate whether logits should be passed instead of
    # probs.
    if not np.isclose(np.sum(probs), 1.0):
      raise ValueError('Probabilities must sum to one.')

  # All probabilities should be the same length.
  if not np.array_equiv([probs.shape for probs in checked_probs_list],
                        checked_probs_list[0].shape):
    raise ValueError('Probability parameters must have the same length.')

  # Labels tensor should only have batch dimension.
  labels.get_shape().assert_has_rank(1)

  # Data tensor should have a batch dimension.
  data_shape = data.get_shape().with_rank_at_least(1)

  # Data and label batch dimensions must be compatible.
  data_shape[0].assert_is_compatible_with(labels.get_shape()[0])

  # Data and labels must have the same, strictly positive batch size. Since we
  # can't assume we know the batch size at graph creation, add runtime checks.
  data_batch_size = array_ops.shape(data)[0]
  labels_batch_size = array_ops.shape(labels)[0]

  data = control_flow_ops.with_dependencies(
      [check_ops.assert_positive(data_batch_size),
       check_ops.assert_equal(data_batch_size, labels_batch_size)],
      data)

  # Label's classes must be integers 0 <= x < num_classes.
  labels = control_flow_ops.with_dependencies(
      [check_ops.assert_integer(labels),
       check_ops.assert_non_negative(labels),
       check_ops.assert_less(labels, math_ops.cast(len(probs), labels.dtype))],
      labels)

  return data, labels, checked_probs_list


def _calculate_rejection_probabilities(init_probs, target_probs):
  """Calculate the per-class rejection rates.

  Args:
    init_probs: The class probabilities of the data.
    target_probs: The desired class proportion in minibatches.
  Returns:
    A list of the per-class rejection probabilities.

  This method is based on solving the following analysis:

  Let F be the probability of a rejection (on any example).
  Let p_i is the proportion of examples in the data in class i (init_probs)
  Let a_i is the rate the rejection sampler should *accept* class i
  Let t_i is the target proportion in the minibatches for class i (target_probs)

  ```
  F = sum_i(p_i * (1-a_i))
    = 1 - sum_i(p_i * a_i)     using sum_i(p_i) = 1
  ```

  An example with class `i` will be accepted if `k` rejections occur, then an
  example with class `i` is seen by the rejector, and it is accepted. This can
  be written as follows:

  ```
  t_i = sum_k=0^inf(F^k * p_i * a_i)
      = p_i * a_j / (1 - F)    using geometric series identity, since 0 <= F < 1
      = p_i * a_i / sum_j(p_j * a_j)        using F from above
  ```

  Note that the following constraints hold:
  ```
  0 <= p_i <= 1, sum_i(p_i) = 1
  0 <= a_i <= 1
  0 <= t_i <= 1, sum_i(t_i) = 1
  ```


  A solution for a_i in terms of the other variabes is the following:
    ```a_i = (t_i / p_i) / max_i[t_i / p_i]```
  """
  # Make list of t_i / p_i.
  ratio_l = [0 if x[0] == 0 else x[1] / x[0] for x in
             zip(init_probs, target_probs)]

  # Calculate list of rejection probabilities.
  max_ratio = max(ratio_l)
  return [1 - ratio / max_ratio for ratio in ratio_l]


def _get_stratified_batch_from_tensors(val, label, reject_probs, batch_size,
                                       queue_threads=3):
  """Reject examples one-at-a-time based on class."""
  # Make rejection probabilities into a tensor so they can be dynamically
  # accessed by tensors.
  reject_probs = constant_op.constant(
      reject_probs, dtype=dtypes.float32, name='rejection_probabilities')

  # Make queue that will have proper class proportions. Contains exactly one
  # batch at a time.
  val_shape = val.get_shape()
  label_shape = label.get_shape()
  final_q = data_flow_ops.FIFOQueue(capacity=batch_size,
                                    shapes=[val_shape, label_shape],
                                    dtypes=[val.dtype, label.dtype],
                                    name='batched_queue')

  # Conditionally enqueue.
  eq_tf = array_ops.reshape(math_ops.greater(
      random_ops.random_uniform([1]),
      array_ops.slice(reject_probs, [label], [1])),
                            [])
  conditional_enqueue = control_flow_ops.cond(
      eq_tf,
      lambda: final_q.enqueue([val, label]),
      control_flow_ops.no_op)
  queue_runner.add_queue_runner(queue_runner.QueueRunner(
      final_q, [conditional_enqueue] * queue_threads))

  return final_q.dequeue_many(batch_size)


def _make_per_class_queues(data, labels, num_classes, queue_capacity,
                           threads_per_queue):
  """Creates per-class-queues based on data and labels."""
  # Create one queue per class.
  queues = []
  per_data_shape = data.get_shape().with_rank_at_least(1)[1:]
  per_data_shape.assert_is_fully_defined()

  for i in range(num_classes):
    q = data_flow_ops.FIFOQueue(capacity=queue_capacity,
                                shapes=per_data_shape, dtypes=[data.dtype],
                                name='stratified_sample_class%d_queue' % i)
    logging_ops.scalar_summary(
        'queue/%s/stratified_sample_class%d' % (q.name, i), q.size())
    queues.append(q)

  # Partition tensors according to labels.
  partitions = data_flow_ops.dynamic_partition(data, labels, num_classes)

  # Enqueue each tensor on the per-class-queue.
  for i in range(num_classes):
    enqueue_op = queues[i].enqueue_many(partitions[i]),
    queue_runner.add_queue_runner(queue_runner.QueueRunner(
        queues[i], [enqueue_op] * threads_per_queue))

  return queues


def _get_batch_from_per_class_queues(per_class_queues, probs, batch_size):
  """Generates batches according to per-class-probabilities."""
  num_classes = probs.size
  # Number of examples per class is governed by a multinomial distribution.
  # Note: multinomial takes unnormalized log probabilities for its first
  # argument, of dimension [batch_size, num_classes].
  examples = random_ops.multinomial(
      np.expand_dims(np.log(probs), 0), batch_size)

  # Prepare the data and label batches.
  val_list = []
  label_list = []
  for i in range(num_classes):
    num_examples = math_ops.reduce_sum(
        math_ops.cast(math_ops.equal(examples, i), dtypes.int32))
    val_list.append(per_class_queues[i].dequeue_many(num_examples))
    label_list.append(array_ops.ones([num_examples], dtype=dtypes.int32) * i)

  # Create a tensor of labels.
  batch_labels = array_ops.concat(0, label_list)
  batch_labels.set_shape([batch_size])

  # Debug instrumentation.
  sample_tags = ['stratified_sample/%s/samples_class%i' % (batch_labels.name, i)
                 for i in range(num_classes)]
  logging_ops.scalar_summary(sample_tags, math_ops.reduce_sum(
      array_ops.one_hot(batch_labels, num_classes), 0))

  return array_ops.concat(0, val_list), batch_labels
