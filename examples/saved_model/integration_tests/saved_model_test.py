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
"""SavedModel integration tests."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from absl.testing import parameterized
import tensorflow.compat.v2 as tf

from tensorflow.examples.saved_model.integration_tests import integration_scripts as scripts
from tensorflow.examples.saved_model.integration_tests import mnist_util
from tensorflow.python.distribute import combinations


class SavedModelTest(scripts.TestCase, parameterized.TestCase):

  def __init__(self, method_name="runTest", has_extra_deps=False):
    super(SavedModelTest, self).__init__(method_name)
    self.has_extra_deps = has_extra_deps

  def skipIfMissingExtraDeps(self):
    """Skip test if it requires extra dependencies.

    b/132234211: The extra dependencies are not available in all environments
    that run the tests, e.g. "tensorflow_hub" is not available from tests
    within "tensorflow" alone. Those tests are instead run by another
    internal test target.
    """
    if not self.has_extra_deps:
      self.skipTest("Missing extra dependencies")

  def test_text_rnn(self):
    export_dir = self.get_temp_dir()
    self.assertCommandSucceeded("export_text_rnn_model", export_dir=export_dir)
    self.assertCommandSucceeded("use_text_rnn_model", model_dir=export_dir)

  def test_rnn_cell(self):
    export_dir = self.get_temp_dir()
    self.assertCommandSucceeded("export_rnn_cell", export_dir=export_dir)
    self.assertCommandSucceeded("use_rnn_cell", model_dir=export_dir)

  def test_text_embedding_in_sequential_keras(self):
    self.skipIfMissingExtraDeps()
    export_dir = self.get_temp_dir()
    self.assertCommandSucceeded(
        "export_simple_text_embedding", export_dir=export_dir)
    self.assertCommandSucceeded(
        "use_model_in_sequential_keras", model_dir=export_dir)

  def test_text_embedding_in_dataset(self):
    if tf.test.is_gpu_available():
      self.skipTest("b/132156097 - fails if there is a gpu available")

    export_dir = self.get_temp_dir()
    self.assertCommandSucceeded(
        "export_simple_text_embedding", export_dir=export_dir)
    self.assertCommandSucceeded(
        "use_text_embedding_in_dataset", model_dir=export_dir)

  def test_mnist_cnn(self):
    self.skipIfMissingExtraDeps()
    export_dir = self.get_temp_dir()
    self.assertCommandSucceeded(
        "export_mnist_cnn", export_dir=export_dir, fast_test_mode="true")
    self.assertCommandSucceeded(
        "use_mnist_cnn", export_dir=export_dir, fast_test_mode="true")

  @combinations.generate(
      combinations.combine(
          named_strategy=list(mnist_util.named_strategies.values())))
  def test_mnist_cnn_with_mirrored_strategy(self, named_strategy):
    self.skipIfMissingExtraDeps()

    name = str(named_strategy)
    if named_strategy:
      combinations.maybe_skip_test(self, named_strategy.required_tpu,
                                   named_strategy.required_gpus)

    export_dir = self.get_temp_dir()
    self.assertCommandSucceeded(
        "export_mnist_cnn",
        export_dir=export_dir,
        fast_test_mode="true")
    self.assertCommandSucceeded(
        "use_mnist_cnn",
        export_dir=export_dir,
        fast_test_mode="true",
        strategy=name)


if __name__ == "__main__":
  scripts.MaybeRunScriptInstead()
  tf.test.main()
