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
# =============================================================================
"""Tests for tpu_test_wrapper.py."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from absl.testing import flagsaver

from tensorflow.python.platform import flags
from tensorflow.python.platform import test
from tensorflow.python.tpu import tpu_test_wrapper


class TPUTestWrapperTest(test.TestCase):

  @flagsaver.flagsaver()
  def test_flags_undefined(self):
    tpu_test_wrapper.maybe_define_flags()

    self.assertIn('tpu', flags.FLAGS)
    self.assertIn('zone', flags.FLAGS)
    self.assertIn('project', flags.FLAGS)
    self.assertIn('model_dir', flags.FLAGS)

  @flagsaver.flagsaver()
  def test_flags_already_defined_not_overridden(self):
    flags.DEFINE_string('tpu', 'tpuname', 'helpstring')
    tpu_test_wrapper.maybe_define_flags()

    self.assertIn('tpu', flags.FLAGS)
    self.assertIn('zone', flags.FLAGS)
    self.assertIn('project', flags.FLAGS)
    self.assertIn('model_dir', flags.FLAGS)
    self.assertEqual(flags.FLAGS.tpu, 'tpuname')

  @flagsaver.flagsaver(bazel_repo_root='tensorflow/python')
  def test_parent_path(self):
    filepath = '/filesystem/path/tensorflow/tensorflow/python/tpu/example_test'
    self.assertEqual(
        tpu_test_wrapper.calculate_parent_python_path(filepath),
        'tensorflow.python.tpu')

  @flagsaver.flagsaver(bazel_repo_root='tensorflow/python')
  def test_parent_path_raises(self):
    filepath = '/bad/path'
    with self.assertRaisesWithLiteralMatch(
        ValueError,
        'Filepath "/bad/path" does not contain repo root "tensorflow/python"'):
      tpu_test_wrapper.calculate_parent_python_path(filepath)

  def test_is_test_class_positive(self):

    class A(test.TestCase):
      pass

    self.assertTrue(tpu_test_wrapper._is_test_class(A))

  def test_is_test_class_negative(self):

    class A(object):
      pass

    self.assertFalse(tpu_test_wrapper._is_test_class(A))

  @flagsaver.flagsaver(wrapped_tpu_test_module_relative='.tpu_test_wrapper_test'
                      )
  def test_move_test_classes_into_scope(self):
    # Test the class importer by having the wrapper module import this test
    # into itself.
    with test.mock.patch.object(
        tpu_test_wrapper, 'calculate_parent_python_path') as mock_parent_path:
      mock_parent_path.return_value = (
          tpu_test_wrapper.__name__.rpartition('.')[0])

      tpu_test_wrapper.move_test_classes_into_scope()

    self.assertEqual(
        tpu_test_wrapper.tpu_test_imported_TPUTestWrapperTest.__name__,
        self.__class__.__name__)

  @flagsaver.flagsaver(test_dir_base='gs://example-bucket/tempfiles')
  def test_set_random_test_dir(self):
    tpu_test_wrapper.maybe_define_flags()
    tpu_test_wrapper.set_random_test_dir()

    self.assertStartsWith(flags.FLAGS.model_dir,
                          'gs://example-bucket/tempfiles/')
    self.assertGreater(
        len(flags.FLAGS.model_dir), len('gs://example-bucket/tempfiles/'))

  @flagsaver.flagsaver(test_dir_base='gs://example-bucket/tempfiles')
  def test_set_random_test_dir_repeatable(self):
    tpu_test_wrapper.maybe_define_flags()
    tpu_test_wrapper.set_random_test_dir()
    first = flags.FLAGS.model_dir
    tpu_test_wrapper.set_random_test_dir()
    second = flags.FLAGS.model_dir

    self.assertNotEqual(first, second)


if __name__ == '__main__':
  test.main()
