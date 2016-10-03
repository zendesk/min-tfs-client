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
"""Demo of the tfdbg curses UI: A TF network computing Fibonacci sequence."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numpy as np
from six.moves import xrange  # pylint: disable=redefined-builtin
import tensorflow as tf

from tensorflow.python.debug import local_cli

flags = tf.app.flags
FLAGS = flags.FLAGS
flags.DEFINE_integer("tensor_size", 30,
                     "Size of tensor. E.g., if the value is 30, the tensors "
                     "will have shape [30, 30].")
flags.DEFINE_integer("length", 20,
                     "Length of the fibonacci sequence to compute.")


def main(_):
  sess = tf.Session()

  # Construct the TensorFlow network.
  n0 = tf.Variable(np.ones([FLAGS.tensor_size] * 2), name="node_00")
  n1 = tf.Variable(np.ones([FLAGS.tensor_size] * 2), name="node_01")

  if FLAGS.length > 100:
    raise ValueError("n is too big.")

  for i in xrange(2, FLAGS.length):
    n0, n1 = n1, tf.add(n0, n1, name="node_%.2d" % i)

  sess.run(tf.initialize_all_variables())

  # Wrap the TensorFlow Session object for debugging.
  sess = local_cli.LocalCLIDebugWrapperSession(sess)

  sess.run(n1)


if __name__ == "__main__":
  tf.app.run()
