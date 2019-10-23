# Copyright 2019 The TensorFlow Authors. All Rights Reserved.
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
"""Test configs for pad."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np
import tensorflow as tf
from tensorflow.lite.testing.zip_test_utils import create_tensor_data
from tensorflow.lite.testing.zip_test_utils import make_zip_of_tests
from tensorflow.lite.testing.zip_test_utils import register_make_test_function


@register_make_test_function()
def make_pad_tests(options):
  """Make a set of tests to do pad."""

  # TODO(nupurgarg): Add test for tf.uint8.
  test_parameters = [
      # 4D:
      {
          "dtype": [tf.int32, tf.int64, tf.float32],
          "input_shape": [[1, 1, 2, 1], [2, 1, 1, 1]],
          "paddings": [[[0, 0], [0, 1], [2, 3], [0, 0]],
                       [[0, 1], [0, 0], [0, 0], [2, 3]]],
          "constant_paddings": [True, False],
      },
      # 2D:
      {
          "dtype": [tf.int32, tf.int64, tf.float32],
          "input_shape": [[1, 2]],
          "paddings": [[[0, 1], [2, 3]]],
          "constant_paddings": [True, False],
      },
      # 1D:
      {
          "dtype": [tf.int32],
          "input_shape": [[1]],
          "paddings": [[[1, 2]]],
          "constant_paddings": [False],
      },
  ]

  def build_graph(parameters):
    """Build a pad graph given `parameters`."""
    input_tensor = tf.compat.v1.placeholder(
        dtype=parameters["dtype"],
        name="input",
        shape=parameters["input_shape"])

    # Get paddings as either a placeholder or constants.
    if parameters["constant_paddings"]:
      paddings = parameters["paddings"]
      input_tensors = [input_tensor]
    else:
      shape = [len(parameters["paddings"]), 2]
      paddings = tf.compat.v1.placeholder(
          dtype=tf.int32, name="padding", shape=shape)
      input_tensors = [input_tensor, paddings]

    out = tf.pad(input_tensor, paddings=paddings)
    return input_tensors, [out]

  def build_inputs(parameters, sess, inputs, outputs):
    values = [
        create_tensor_data(parameters["dtype"], parameters["input_shape"])
    ]
    if not parameters["constant_paddings"]:
      values.append(np.array(parameters["paddings"]))
    return values, sess.run(outputs, feed_dict=dict(zip(inputs, values)))

  make_zip_of_tests(options, test_parameters, build_graph, build_inputs)
