# Copyright 2020 The TensorFlow Authors. All Rights Reserved.
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
"""Tests for tensorflow.compiler.mlir.tfr.examples.customization.ops_defs.py."""

import os
import tensorflow as tf

from tensorflow.compiler.mlir.tfr.python import test_utils
from tensorflow.python.framework import test_ops
from tensorflow.python.platform import test


class TestOpsDefsTest(test_utils.OpsDefsTest):

  def test_test_ops(self):
    attr = tf.function(test_ops.test_attr)(tf.float32)
    self.assertAllClose(attr.numpy(), 100.0)


if __name__ == '__main__':
  os.environ['TF_MLIR_TFR_LIB_DIR'] = (
      'tensorflow/compiler/mlir/tfr/examples/customization')
  test.main()
