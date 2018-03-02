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
"""Tests for builtins module."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import sys

import six

from tensorflow.contrib.py2tf.utils import builtins
from tensorflow.python.framework import constant_op
from tensorflow.python.platform import test


class BuiltinsTest(test.TestCase):

  def test_dynamic_len_tf_scalar(self):
    a = constant_op.constant(1)

    with self.assertRaises(ValueError):
      with self.test_session() as sess:
        sess.run(builtins.dynamic_builtin(len, a))

  def test_dynamic_len_tf_array(self):
    a = constant_op.constant([1, 2, 3])

    with self.test_session() as sess:
      self.assertEqual(3, sess.run(builtins.dynamic_builtin(len, a)))

  def test_dynamic_len_tf_matrix(self):
    a = constant_op.constant([[1, 2], [3, 4]])

    with self.test_session() as sess:
      self.assertEqual(2, sess.run(builtins.dynamic_builtin(len, a)))

  def test_dynamic_len_py_list(self):
    a = [3] * 5

    self.assertEqual(5, builtins.dynamic_builtin(len, a))

  def test_dynamic_print_tf(self):
    try:
      out_capturer = six.StringIO()
      sys.stdout = out_capturer
      with self.test_session() as sess:
        sess.run(builtins.dynamic_print('test message', 1))
        self.assertEqual(out_capturer.getvalue(), 'test message 1\n')
    finally:
      sys.stdout = sys.__stdout__

  def test_dynamic_print_complex(self):
    try:
      out_capturer = six.StringIO()
      sys.stdout = out_capturer
      with self.test_session() as sess:
        sess.run(builtins.dynamic_print('test message', [1, 2]))
        self.assertEqual(out_capturer.getvalue(), 'test message [1, 2]\n')
    finally:
      sys.stdout = sys.__stdout__


if __name__ == '__main__':
  test.main()
