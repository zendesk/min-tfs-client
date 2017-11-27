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
"""Tests for tf.contrib.gan.python.features.tensor_pool."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np

from tensorflow.contrib.gan.python.features.python import tensor_pool_impl as tensor_pool
from tensorflow.python.framework import dtypes
from tensorflow.python.ops import array_ops
from tensorflow.python.platform import test


class TensorPoolTest(test.TestCase):

  def test_pool_unknown_input_shape(self):
    """Checks that `input_value` can have unknown shape."""
    input_value = array_ops.placeholder(
        dtype=dtypes.int32, shape=[None, None, 3])
    output_value = tensor_pool.tensor_pool(input_value, pool_size=10)

    with self.test_session(use_gpu=True) as session:
      for i in range(10):
        session.run(output_value, {input_value: [[[i] * 3]]})
        session.run(output_value, {input_value: [[[i] * 3] * 2]})
        session.run(output_value, {input_value: [[[i] * 3] * 5] * 2})

  def test_pool_sequence(self):
    """Checks that values are pooled and returned maximally twice."""
    input_value = array_ops.placeholder(dtype=dtypes.int32, shape=[])
    output_value = tensor_pool.tensor_pool(input_value, pool_size=10)

    with self.test_session(use_gpu=True) as session:
      outs = []
      for i in range(50):
        out = session.run(output_value, {input_value: i})
        outs.append(out)
        self.assertLessEqual(out, i)

      _, counts = np.unique(outs, return_counts=True)
      # Check that each value is returned maximally twice.
      self.assertTrue((counts <= 2).all())

  def test_never_pool(self):
    """Checks that setting `pooling_probability` to zero works."""
    input_value = array_ops.placeholder(dtype=dtypes.int32, shape=[])
    output_value = tensor_pool.tensor_pool(
        input_value, pool_size=10, pooling_probability=0.0)

    with self.test_session(use_gpu=True) as session:
      for i in range(50):
        out = session.run(output_value, {input_value: i})
        self.assertEqual(out, i)

  def test_pooling_probability(self):
    """Checks that `pooling_probability` works."""
    input_value = array_ops.placeholder(dtype=dtypes.int32, shape=[])
    pool_size = 10
    pooling_probability = 0.2
    output_value = tensor_pool.tensor_pool(
        input_value,
        pool_size=pool_size,
        pooling_probability=pooling_probability)

    with self.test_session(use_gpu=True) as session:
      not_pooled = 0
      total = 1000
      for i in range(total):
        out = session.run(output_value, {input_value: i})
        if out == i:
          not_pooled += 1
      self.assertAllClose(
          (not_pooled - pool_size) / (total - pool_size),
          1 - pooling_probability,
          atol=0.03)


if __name__ == '__main__':
  test.main()
