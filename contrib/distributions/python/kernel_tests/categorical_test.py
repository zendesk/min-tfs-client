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
"""Tests for Categorical distribution."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np
import tensorflow as tf


def make_categorical(batch_shape, num_classes):
  histogram = np.random.random(list(batch_shape) + [num_classes])
  histogram /= np.sum(histogram, axis=tuple(range(len(batch_shape))))
  histogram = tf.constant(histogram, dtype=tf.float32)
  return tf.contrib.distributions.Categorical(tf.log(histogram))


class CategoricalTest(tf.test.TestCase):

  def testLogits(self):
    logits = np.log([0.2, 0.8])
    dist = tf.contrib.distributions.Categorical(logits)
    with self.test_session():
      self.assertAllClose(logits, dist.logits.eval())

  def testShapes(self):
    with self.test_session():
      for batch_shape in ([], [1], [2, 3, 4]):
        dist = make_categorical(batch_shape, 10)
        self.assertAllEqual(batch_shape, dist.get_batch_shape().as_list())
        self.assertAllEqual(batch_shape, dist.batch_shape().eval())
        self.assertAllEqual([], dist.get_event_shape().as_list())
        self.assertAllEqual([], dist.event_shape().eval())

  def testDtype(self):
    dist = make_categorical([], 5)
    self.assertEqual(dist.dtype, tf.int64)
    self.assertEqual(dist.dtype, dist.sample(5).dtype)
    self.assertEqual(dist.dtype, dist.mode().dtype)
    self.assertEqual(dist.logits.dtype, tf.float32)
    self.assertEqual(dist.logits.dtype, dist.entropy().dtype)
    self.assertEqual(dist.logits.dtype, dist.pmf(0).dtype)
    self.assertEqual(dist.logits.dtype, dist.log_pmf(0).dtype)

  def testPMF(self):
    histograms = [[0.2, 0.8], [0.6, 0.4]]
    dist = tf.contrib.distributions.Categorical(tf.log(histograms))
    with self.test_session():
      self.assertAllClose(dist.pmf(0).eval(), [0.2, 0.6])
      self.assertAllClose(dist.pmf([0, 1]).eval(), histograms)

  def testLogPMF(self):
    logits = np.log([[0.2, 0.8], [0.6, 0.4]])
    dist = tf.contrib.distributions.Categorical(logits)
    with self.test_session():
      self.assertAllClose(dist.log_pmf(0).eval(), np.log([0.2, 0.6]))
      self.assertAllClose(dist.log_pmf([0, 1]).eval(), logits)

  def testEntropyNoBatch(self):
    logits = np.log([0.2, 0.8])
    dist = tf.contrib.distributions.Categorical(logits)
    with self.test_session():
      self.assertAllClose(
          dist.entropy().eval(),
          -(0.2 * np.log(0.2) + 0.8 * np.log(0.8)))

  def testEntropyWithBatch(self):
    logits = np.log([[0.2, 0.8], [0.6, 0.4]])
    dist = tf.contrib.distributions.Categorical(logits)
    with self.test_session():
      self.assertAllClose(dist.entropy().eval(),
                          [-(0.2 * np.log(0.2) + 0.8 * np.log(0.8)),
                           -(0.6 * np.log(0.6) + 0.4 * np.log(0.4))])

  def testSample(self):
    with self.test_session():
      histograms = [[[0.2, 0.8], [0.4, 0.6]]]
      dist = tf.contrib.distributions.Categorical(tf.log(histograms))
      n = 10000
      samples = dist.sample(n, seed=123)
      samples.set_shape([n, 1, 2])
      self.assertEqual(samples.dtype, tf.int64)
      sample_values = samples.eval()
      self.assertFalse(np.any(sample_values < 0))
      self.assertFalse(np.any(sample_values > 1))
      self.assertAllClose(
          [[0.2, 0.4]], np.mean(sample_values == 0, axis=0), atol=1e-2)
      self.assertAllClose(
          [[0.8, 0.6]], np.mean(sample_values == 1, axis=0), atol=1e-2)

  def testMode(self):
    with self.test_session():
      histograms = [[[0.2, 0.8], [0.6, 0.4]]]
      dist = tf.contrib.distributions.Categorical(tf.log(histograms))
      self.assertAllEqual(dist.mode().eval(), [[1, 0]])

if __name__ == "__main__":
  tf.test.main()
