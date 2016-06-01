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

"""A Series represents a deferred Tensor computation in a DataFrame."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from abc import ABCMeta


class Series(object):
  """A single output series.

  Represents the deferred construction of a graph that computes the series
  values.

  Note every `Series` should be a `TransformedSeries`, except when mocked.
  """

  __metaclass__ = ABCMeta

  def build(self, cache):
    """Returns a Tensor."""
    raise NotImplementedError()


class TransformedSeries(Series):
  """A `Series` that results from applying a `Transform` to a list of inputs."""

  def __init__(self, input_series, transform, output_name):
    super(TransformedSeries, self).__init__()
    self._input_series = input_series
    self._transform = transform
    self._output_name = output_name

    if output_name is None:
      raise ValueError("output_name must be provided")

    if len(input_series) != transform.input_valency:
      raise ValueError("Expected %s input Series but received %s." %
                       (transform.input_valency, len(input_series)))

    self._repr = TransformedSeries.make_repr(
        self._input_series, self._transform, self._output_name)

  def build(self, cache=None):
    if cache is None:
      cache = {}
    all_outputs = self._transform.apply_transform(self._input_series, cache)
    return getattr(all_outputs, self._output_name)

  def __repr__(self):
    return self._repr

  # Note we need to generate series reprs from Transform, without needing the
  # series themselves.  So we just make this public.  Alternatively we could
  # create throwaway series just in order to call repr() on them.
  @staticmethod
  def make_repr(input_series, transform, output_name):
    """Generate a key for caching Tensors produced for a TransformedSeries.

    Generally we a need a deterministic unique key representing which transform
    was applied to which inputs, and which output was selected.

    Args:
      input_series: an iterable of input `Series` for the `Transform`
      transform: the `Transform` being applied
      output_name: the name of the specific output from the `Transform` that is
        to be cached

    Returns:
      A string suitable for use as a cache key for Tensors produced via a
        TransformedSeries
    """
    input_series_keys = [repr(series) for series in input_series]
    input_series_keys_joined = ", ".join(input_series_keys)

    return "%s(%s)[%s]" % (
        repr(transform), input_series_keys_joined, output_name)


