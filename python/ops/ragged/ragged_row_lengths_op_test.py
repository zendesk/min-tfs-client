# Copyright 2018 The TensorFlow Authors. All Rights Reserved.
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
"""Tests for ragged.row_lengths."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from absl.testing import parameterized

from tensorflow.python.framework import test_util
from tensorflow.python.ops import ragged
from tensorflow.python.platform import googletest


class RaggedRowLengthsOp(test_util.TensorFlowTestCase, parameterized.TestCase):

  @parameterized.parameters([
      # Docstring Example
      dict(
          rt_input=[[[3, 1, 4], [1]], [], [[5, 9], [2]], [[6]], []],
          expected=[2, 0, 2, 1, 0]),
      dict(
          rt_input=[[[3, 1, 4], [1]], [], [[5, 9], [2]], [[6]], []],
          axis=2,
          expected=[[3, 1], [], [2, 1], [1], []]),

      # 1D tensor
      dict(
          rt_input=[1, 2, 3, 4, 5],
          ragged_rank=0,
          axis=0,
          expected=5),

      # 2D Tensor (0 ragged dimensions)
      dict(
          rt_input=[[1, 2], [3, 4], [5, 6], [7, 8]],
          ragged_rank=0,
          expected=[2, 2, 2, 2]),
      dict(
          rt_input=[[1, 2], [3, 4], [5, 6], [7, 8]],
          ragged_rank=0,
          axis=0,
          expected=4),

      # 2D Tensor (1 ragged dimension)
      dict(
          rt_input=[['a'], ['b', 'c', 'd'], ['e'], [], ['f']],
          expected=[1, 3, 1, 0, 1]),
      dict(
          rt_input=[['a'], ['b', 'c', 'd'], ['e'], [], ['f']],
          axis=0,
          expected=5),
      dict(
          rt_input=[['a', 'b', 'c', 'd', 'e', 'f', 'g']],
          expected=[7]),
      dict(
          rt_input=[[], ['a', 'b', 'c', 'd', 'e', 'f', 'g'], []],
          expected=[0, 7, 0]),
      dict(
          rt_input=[],
          ragged_rank=1,
          expected=[]),
      dict(
          rt_input=[],
          ragged_rank=1,
          axis=0,
          expected=0),

      # 3D Tensor (0 ragged dimensions)
      dict(
          rt_input=[[[1, 2], [3, 4], [5, 6]], [[7, 8], [9, 10], [11, 12]]],
          ragged_rank=0,
          axis=0,
          expected=2),
      dict(
          rt_input=[[[1, 2], [3, 4], [5, 6]], [[7, 8], [9, 10], [11, 12]]],
          ragged_rank=0,
          axis=1,
          expected=[3, 3]),
      dict(
          rt_input=[[[1, 2], [3, 4], [5, 6]], [[7, 8], [9, 10], [11, 12]]],
          ragged_rank=0,
          axis=2,
          expected=[[2, 2, 2], [2, 2, 2]],
          expected_ragged_rank=0),

      # 3D Tensor (1 ragged dimension)
      dict(
          rt_input=[[[1, 2], [3, 4], [5, 6]], [[7, 8], [9, 10]]],
          ragged_rank=1,
          axis=0,
          expected=2),
      dict(
          rt_input=[[[1, 2], [3, 4], [5, 6]], [[7, 8], [9, 10]]],
          ragged_rank=1,
          axis=1,
          expected=[3, 2]),
      dict(
          rt_input=[[[1, 2], [3, 4], [5, 6]], [[7, 8], [9, 10]]],
          ragged_rank=1,
          axis=2,
          expected=[[2, 2, 2], [2, 2]],
          expected_ragged_rank=1),

      # 3D Tensor (2 ragged dimensions)
      dict(
          rt_input=[[[1, 2], [3, 4, 5], []], [[6, 7, 8, 9], [10]]],
          axis=0,
          expected=2),
      dict(
          rt_input=[[[1, 2], [3, 4, 5], []], [[6, 7, 8, 9], [10]]],
          axis=-3,
          expected=2),
      dict(
          rt_input=[[[1, 2], [3, 4, 5], []], [[6, 7, 8, 9], [10]]],
          axis=1,
          expected=[3, 2]),
      dict(
          rt_input=[[[1, 2], [3, 4, 5], []], [[6, 7, 8, 9], [10]]],
          axis=-2,
          expected=[3, 2]),
      dict(
          rt_input=[[[1, 2], [3, 4, 5], []], [[6, 7, 8, 9], [10]]],
          axis=2,
          expected=[[2, 3, 0], [4, 1]],
          expected_ragged_rank=1),
      dict(
          rt_input=[[[1, 2], [3, 4, 5], []], [[6, 7, 8, 9], [10]]],
          axis=-1,
          expected=[[2, 3, 0], [4, 1]],
          expected_ragged_rank=1),
  ])  # pyformat: disable
  @test_util.run_deprecated_v1
  def testRowLengths(self,
                     rt_input,
                     expected,
                     axis=1,
                     ragged_rank=None,
                     expected_ragged_rank=None):
    rt = ragged.constant(rt_input, ragged_rank=ragged_rank)
    lengths = ragged.row_lengths(rt, axis)
    with self.test_session():
      self.assertEqual(lengths.eval().tolist(), expected)
      if expected_ragged_rank is not None:
        if isinstance(lengths, ragged.RaggedTensor):
          self.assertEqual(lengths.ragged_rank, expected_ragged_rank)
        else:
          self.assertEqual(0, expected_ragged_rank)

  @parameterized.parameters([
      dict(
          rt_input=10,
          exception=ValueError,
          message='rt_input may not be a scalar.'),
      dict(
          rt_input=[10, 20],
          axis=1,
          exception=ValueError,
          message='axis=1 out of bounds: expected -1<=axis<1.'),
      dict(
          rt_input=[[2, 3, 0], [4, 1, 2]],
          axis=-3,
          exception=ValueError,
          message='axis=-3 out of bounds: expected -2<=axis<2.'),
  ])
  def testErrors(self, rt_input, exception, message, axis=1):
    with self.assertRaisesRegexp(exception, message):
      ragged.row_lengths(rt_input, axis)


if __name__ == '__main__':
  googletest.main()
