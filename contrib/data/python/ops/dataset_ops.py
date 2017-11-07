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
"""Python wrappers for Datasets and Iterators."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.contrib.data.python.ops import batching
from tensorflow.contrib.data.python.ops import enumerate_ops
from tensorflow.contrib.data.python.ops import error_ops
from tensorflow.contrib.data.python.ops import gen_dataset_ops
from tensorflow.contrib.data.python.ops import grouping

from tensorflow.contrib.util import loader
from tensorflow.python.data.ops import dataset_ops
from tensorflow.python.data.util import nest
from tensorflow.python.ops import gen_io_ops
from tensorflow.python.platform import resource_loader
from tensorflow.python.util import deprecation


_dataset_ops = loader.load_op_library(
    resource_loader.get_path_to_datafile("../../_dataset_ops.so"))


class Dataset(dataset_ops.Dataset):
  """Represents a potentially large set of elements.

  A `Dataset` can be used to represent an input pipeline as a
  collection of elements (nested structures of tensors) and a "logical
  plan" of transformations that act on those elements.
  """

  def __init__(self, dataset):
    super(Dataset, self).__init__()
    self._dataset = dataset

  @deprecation.deprecated(None, "Use `ds._as_variant_tensor()`.")
  def make_dataset_resource(self):
    return self._as_variant_tensor()

  def _as_variant_tensor(self):
    return self._dataset._as_variant_tensor()  # pylint: disable=protected-access

  @property
  def output_shapes(self):
    return self._dataset.output_shapes

  @property
  def output_types(self):
    return self._dataset.output_types

  @staticmethod
  @deprecation.deprecated(None, "Use `tf.data.Dataset.from_tensors()`.")
  def from_tensors(tensors):
    """Creates a `Dataset` with a single element, comprising the given tensors.

    Args:
      tensors: A nested structure of tensors.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.TensorDataset(tensors))

  @staticmethod
  @deprecation.deprecated(None, "Use `tf.data.Dataset.from_tensor_slices()`.")
  def from_tensor_slices(tensors):
    """Creates a `Dataset` whose elements are slices of the given tensors.

    Args:
      tensors: A nested structure of tensors, each having the same size in the
        0th dimension.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.TensorSliceDataset(tensors))

  @staticmethod
  @deprecation.deprecated(None,
                          "Use `tf.data.Dataset.from_sparse_tensor_slices()`.")
  def from_sparse_tensor_slices(sparse_tensor):
    """Splits each rank-N `tf.SparseTensor` in this dataset row-wise.

    Args:
      sparse_tensor: A `tf.SparseTensor`.

    Returns:
      A `Dataset` of rank-(N-1) sparse tensors.
    """
    return Dataset(dataset_ops.SparseTensorSliceDataset(sparse_tensor))

  @staticmethod
  @deprecation.deprecated(None, "Use `tf.data.Dataset.from_generator()`.")
  def from_generator(generator, output_types, output_shapes=None):
    """Creates a `Dataset` whose elements are generated by `generator`.

    The `generator` argument must be a callable object that returns
    an object that support the `iter()` protocol (e.g. a generator function).
    The elements generated by `generator` must be compatible with the given
    `output_types` and (optional) `output_shapes` arguments.

    For example:

    ```python
    import itertools

    def gen():
      for i in itertools.count(1):
        yield (i, [1] * i)

    ds = Dataset.from_generator(
        gen, (tf.int64, tf.int64), (tf.TensorShape([]), tf.TensorShape([None])))
    value = ds.make_one_shot_iterator().get_next()

    sess.run(value)  # (1, array([1]))
    sess.run(value)  # (2, array([1, 1]))
    ```

    Args:
      generator: A callable object that takes no arguments and returns an
        object that supports the `iter()` protocol.
      output_types: A nested structure of `tf.DType` objects corresponding to
        each component of an element yielded by `generator`.
      output_shapes: (Optional.) A nested structure of `tf.TensorShape`
        objects corresponding to each component of an element yielded by
        `generator`.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.Dataset.from_generator(
        generator, output_types, output_shapes))

  @staticmethod
  @deprecation.deprecated(None, "Use `tf.data.Dataset.range()`.")
  def range(*args):
    """Creates a `Dataset` of a step-separated range of values.

    For example:

    ```python
    Dataset.range(5) == [0, 1, 2, 3, 4]
    Dataset.range(2, 5) == [2, 3, 4]
    Dataset.range(1, 5, 2) == [1, 3]
    Dataset.range(1, 5, -2) == []
    Dataset.range(5, 1) == []
    Dataset.range(5, 1, -2) == [5, 3]
    ```

    Args:
      *args: follow same semantics as python's xrange.
        len(args) == 1 -> start = 0, stop = args[0], step = 1
        len(args) == 2 -> start = args[0], stop = args[1], step = 1
        len(args) == 3 -> start = args[0], stop = args[1, stop = args[2]

    Returns:
      A `RangeDataset`.

    Raises:
      ValueError: if len(args) == 0.
    """
    return Dataset(dataset_ops.RangeDataset(*args))

  @staticmethod
  @deprecation.deprecated(None, "Use `tf.data.Dataset.zip()`.")
  def zip(datasets):
    """Creates a `Dataset` by zipping together the given datasets.

    This method has similar semantics to the built-in `zip()` function
    in Python, with the main difference being that the `datasets`
    argument can be an arbitrary nested structure of `Dataset` objects.
    For example:

    ```python
    # NOTE: The following examples use `{ ... }` to represent the
    # contents of a dataset.
    a = { 1, 2, 3 }
    b = { 4, 5, 6 }
    c = { (7, 8), (9, 10), (11, 12) }
    d = { 13, 14 }

    # The nested structure of the `datasets` argument determines the
    # structure of elements in the resulting dataset.
    Dataset.zip((a, b)) == { (1, 4), (2, 5), (3, 6) }
    Dataset.zip((b, a)) == { (4, 1), (5, 2), (6, 3) }

    # The `datasets` argument may contain an arbitrary number of
    # datasets.
    Dataset.zip((a, b, c)) == { (1, 4, (7, 8)),
                                (2, 5, (9, 10)),
                                (3, 6, (11, 12)) }

    # The number of elements in the resulting dataset is the same as
    # the size of the smallest dataset in `datasets`.
    Dataset.zip((a, d)) == { (1, 13), (2, 14) }
    ```

    Args:
      datasets: A nested structure of datasets.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.ZipDataset(datasets))

  def concatenate(self, dataset):
    """Creates a `Dataset` by concatenating given dataset with this dataset.

    ```python
    # NOTE: The following examples use `{ ... }` to represent the
    # contents of a dataset.
    a = { 1, 2, 3 }
    b = { 4, 5, 6, 7 }

    # Input dataset and dataset to be concatenated should have same
    # nested structures and output types.
    # c = { (8, 9), (10, 11), (12, 13) }
    # d = { 14.0, 15.0, 16.0 }
    # a.concatenate(c) and a.concatenate(d) would result in error.

    a.concatenate(b) == { 1, 2, 3, 4, 5, 6, 7 }
    ```

    Args:
      dataset: `Dataset` to be concatenated.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.ConcatenateDataset(self._dataset, dataset))

  def prefetch(self, buffer_size):
    """Creates a `Dataset` that prefetches elements from this dataset.

    Args:
      buffer_size: A `tf.int64` scalar `tf.Tensor`, representing the
        maximum number elements that will be buffered when prefetching.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.PrefetchDataset(self._dataset, buffer_size))

  @staticmethod
  @deprecation.deprecated(None, "Use `tf.data.Dataset.list_files()`.")
  def list_files(file_pattern):
    """A dataset of all files matching a pattern.

    Example:
      If we had the following files on our filesystem:
        - /path/to/dir/a.txt
        - /path/to/dir/b.py
        - /path/to/dir/c.py
      If we pass "/path/to/dir/*.py" as the directory, the dataset would
      produce:
        - /path/to/dir/b.py
        - /path/to/dir/c.py

    Args:
      file_pattern: A string or scalar string `tf.Tensor`, representing
        the filename pattern that will be matched.

    Returns:
     A `Dataset` of strings corresponding to file names.
    """
    return Dataset.from_tensor_slices(gen_io_ops.matching_files(file_pattern))

  def repeat(self, count=None):
    """Repeats this dataset `count` times.

    Args:
      count: (Optional.) A `tf.int64` scalar `tf.Tensor`, representing the
        number of times the elements of this dataset should be repeated. The
        default behavior (if `count` is `None` or `-1`) is for the elements to
        be repeated indefinitely.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.RepeatDataset(self._dataset, count))

  @deprecation.deprecated(
      None, "Use `ds.apply(tf.contrib.data.enumerate_dataset())`.")
  def enumerate(self, start=0):
    """Deprecated: Use `Dataset.apply(tf.contrib.data.enumerate_dataset(..)`."""

    return self.apply(enumerate_ops.enumerate_dataset(start))

  def shuffle(self, buffer_size, seed=None):
    """Randomly shuffles the elements of this dataset.

    Args:
      buffer_size: A `tf.int64` scalar `tf.Tensor`, representing the
        number of elements from this dataset from which the new
        dataset will sample.
      seed: (Optional.) A `tf.int64` scalar `tf.Tensor`, representing the
        random seed that will be used to create the distribution. See
        @{tf.set_random_seed} for behavior.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.ShuffleDataset(self._dataset, buffer_size, seed))

  def cache(self, filename=""):
    """Caches the elements in this dataset.

    Args:
      filename: A `tf.string` scalar `tf.Tensor`, representing the name of a
        directory on the filesystem to use for caching tensors in this Dataset.
        If a filename is not provided, the dataset will be cached in memory.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.CacheDataset(self._dataset, filename))

  def take(self, count):
    """Creates a `Dataset` with at most `count` elements from this dataset.

    Args:
      count: A `tf.int64` scalar `tf.Tensor`, representing the number of
        elements of this dataset that should be taken to form the new dataset.
        If `count` is -1, or if `count` is greater than the size of this
        dataset, the new dataset will contain all elements of this dataset.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.TakeDataset(self._dataset, count))

  def skip(self, count):
    """Creates a `Dataset` that skips `count` elements from this dataset.

    Args:
      count: A `tf.int64` scalar `tf.Tensor`, representing the number
        of elements of this dataset that should be skipped to form the
        new dataset.  If `count` is greater than the size of this
        dataset, the new dataset will contain no elements.  If `count`
        is -1, skips the entire dataset.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.SkipDataset(self._dataset, count))

  def shard(self, num_shards, index):
    """Creates a `Dataset` that includes only 1/`num_shards` of this dataset.

    This dataset operator is very useful when running distributed training, as
    it allows each worker to read a unique subset.

    When reading a single input file, you can skip elements as follows:

    ```python
    d = tf.contrib.data.TFRecordDataset(FLAGS.input_file)
    d = d.shard(FLAGS.num_workers, FLAGS.worker_index)
    d = d.repeat(FLAGS.num_epochs)
    d = d.shuffle(FLAGS.shuffle_buffer_size)
    d = d.map(parser_fn, num_parallel_calls=FLAGS.num_map_threads)
    ```

    Important caveats:

    - Be sure to shard before you use any randomizing operator (such as
      shuffle).
    - Generally it is best if the shard operator is used early in the dataset
      pipeline. For example, when reading from a set of TFRecord files, shard
      before converting the dataset to input samples. This avoids reading every
      file on every worker. The following is an example of an efficient
      sharding strategy within a complete pipeline:

    ```python
    d = Dataset.list_files(FLAGS.pattern)
    d = d.shard(FLAGS.num_workers, FLAGS.worker_index)
    d = d.repeat(FLAGS.num_epochs)
    d = d.shuffle(FLAGS.shuffle_buffer_size)
    d = d.repeat()
    d = d.interleave(tf.contrib.data.TFRecordDataset,
                     cycle_length=FLAGS.num_readers, block_length=1)
    d = d.map(parser_fn, num_parallel_calls=FLAGS.num_map_threads)
    ```

    Args:
      num_shards: A `tf.int64` scalar `tf.Tensor`, representing the number of
        shards operating in parallel.
      index: A `tf.int64` scalar `tf.Tensor`, representing the worker index.

    Returns:
      A `Dataset`.

    Raises:
      ValueError: if `num_shards` or `index` are illegal values. Note: error
        checking is done on a best-effort basis, and aren't guaranteed to be
        caught upon dataset creation. (e.g. providing in a placeholder tensor
        bypasses the early checking, and will instead result in an error during
        a session.run call.)
    """
    return Dataset(self._dataset.shard(num_shards, index))

  @deprecation.deprecated(
      None, "Use `ds.apply(tf.contrib.data.ignore_errors())`.")
  def ignore_errors(self):
    """Deprecated: Use `Dataset.apply(tf.contrib.data.ignore_errors())`."""

    return self.apply(error_ops.ignore_errors())

  def batch(self, batch_size):
    """Combines consecutive elements of this dataset into batches.

    Args:
      batch_size: A `tf.int64` scalar `tf.Tensor`, representing the number of
        consecutive elements of this dataset to combine in a single batch.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.BatchDataset(self._dataset, batch_size))

  def padded_batch(self, batch_size, padded_shapes, padding_values=None):
    """Combines consecutive elements of this dataset into padded batches.

    Like `Dataset.dense_to_sparse_batch()`, this method combines
    multiple consecutive elements of this dataset, which might have
    different shapes, into a single element. The tensors in the
    resulting element have an additional outer dimension, and are
    padded to the respective shape in `padded_shapes`.

    Args:
      batch_size: A `tf.int64` scalar `tf.Tensor`, representing the number of
        consecutive elements of this dataset to combine in a single batch.
      padded_shapes: A nested structure of `tf.TensorShape` or
        `tf.int64` vector tensor-like objects representing the shape
        to which the respective component of each input element should
        be padded prior to batching. Any unknown dimensions
        (e.g. `tf.Dimension(None)` in a `tf.TensorShape` or `-1` in a
        tensor-like object) will be padded to the maximum size of that
        dimension in each batch.
      padding_values: (Optional.) A nested structure of scalar-shaped
        `tf.Tensor`, representing the padding values to use for the
        respective components.  Defaults are `0` for numeric types and
        the empty string for string types.

    Returns:
      A `Dataset`.
    """
    return Dataset(
        dataset_ops.PaddedBatchDataset(self._dataset, batch_size, padded_shapes,
                                       padding_values))

  @deprecation.deprecated(
      None, "Use `ds.apply(tf.contrib.data.dense_to_sparse_batch())`.")
  def dense_to_sparse_batch(self, batch_size, row_shape):
    """Use: `Dataset.apply(tf.contrib.data.dense_to_sparse_batch(...))`."""

    return self.apply(batching.dense_to_sparse_batch(batch_size, row_shape))

  @deprecation.deprecated(
      None, "Use `ds.apply(tf.contrib.data.group_by_window())`.")
  def group_by_window(self, key_func, reduce_func, window_size):
    """Deprecated: Use `Dataset.apply(tf.contrib.data.group_by_window(...))`."""

    return self.apply(
        grouping.group_by_window(key_func, reduce_func, window_size))

  @deprecation.deprecated_args(
      None,
      "Replace `num_threads=T` with `num_parallel_calls=T`. Replace "
      "`output_buffer_size=N` with `ds.prefetch(N)` on the returned dataset.",
      "num_threads", "output_buffer_size")
  def map(self,
          map_func,
          num_threads=None,
          output_buffer_size=None,
          num_parallel_calls=None):
    """Maps `map_func` across this datset.

    Args:
      map_func: A function mapping a nested structure of tensors (having
        shapes and types defined by `self.output_shapes` and
       `self.output_types`) to another nested structure of tensors.
      num_threads: (Optional.) Deprecated, use `num_parallel_calls` instead.
      output_buffer_size: (Optional.) A `tf.int64` scalar `tf.Tensor`,
        representing the maximum number of processed elements that will be
        buffered.
      num_parallel_calls: (Optional.) A `tf.int32` scalar `tf.Tensor`,
        representing the number elements to process in parallel. If not
        specified, elements will be processed sequentially.

    Returns:
      A `Dataset`.
    """
    if num_threads is None and num_parallel_calls is None:
      ret = Dataset(dataset_ops.MapDataset(self._dataset, map_func))
    else:
      if num_threads is None:
        ret = Dataset(
            dataset_ops.ParallelMapDataset(self._dataset, map_func,
                                           num_parallel_calls))
      else:
        ret = Dataset(
            dataset_ops.ParallelMapDataset(self._dataset, map_func,
                                           num_threads))
    if output_buffer_size is not None:
      ret = ret.prefetch(output_buffer_size)
    return ret

  def flat_map(self, map_func):
    """Maps `map_func` across this dataset and flattens the result.

    Args:
      map_func: A function mapping a nested structure of tensors (having shapes
        and types defined by `self.output_shapes` and `self.output_types`) to a
        `Dataset`.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.FlatMapDataset(self._dataset, map_func))

  def interleave(self, map_func, cycle_length, block_length=1):
    """Maps `map_func` across this dataset, and interleaves the results.

    For example, you can use `Dataset.interleave()` to process many input files
    concurrently:

    ```python
    # Preprocess 4 files concurrently, and interleave blocks of 16 records from
    # each file.
    filenames = ["/var/data/file1.txt", "/var/data/file2.txt", ...]
    dataset = (Dataset.from_tensor_slices(filenames)
               .interleave(lambda x:
                   TextLineDataset(x).map(parse_fn, num_parallel_calls=1),
                   cycle_length=4, block_length=16))
    ```

    The `cycle_length` and `block_length` arguments control the order in which
    elements are produced. `cycle_length` controls the number of input elements
    that are processed concurrently. If you set `cycle_length` to 1, this
    transformation will handle one input element at a time, and will produce
    identical results = to @{tf.contrib.data.Dataset.flat_map}. In general,
    this transformation will apply `map_func` to `cycle_length` input elements,
    open iterators on the returned `Dataset` objects, and cycle through them
    producing `block_length` consecutive elements from each iterator, and
    consuming the next input element each time it reaches the end of an
    iterator.

    For example:

    ```python
    # NOTE: The following examples use `{ ... }` to represent the
    # contents of a dataset.
    a = { 1, 2, 3, 4, 5 }

    # NOTE: New lines indicate "block" boundaries.
    a.interleave(lambda x: Dataset.from_tensors(x).repeat(6),
                 cycle_length=2, block_length=4) == {
        1, 1, 1, 1,
        2, 2, 2, 2,
        1, 1,
        2, 2,
        3, 3, 3, 3,
        4, 4, 4, 4,
        3, 3,
        4, 4,
        5, 5, 5, 5,
        5, 5,
    }
    ```

    NOTE: The order of elements yielded by this transformation is
    deterministic, as long as `map_func` is a pure function. If
    `map_func` contains any stateful operations, the order in which
    that state is accessed is undefined.

    Args:
      map_func: A function mapping a nested structure of tensors (having shapes
        and types defined by `self.output_shapes` and `self.output_types`) to a
        `Dataset`.
      cycle_length: The number of elements from this dataset that will be
        processed concurrently.
      block_length: The number of consecutive elements to produce from each
        input element before cycling to another input element.

    Returns:
      A `Dataset`.
    """
    return Dataset(
        dataset_ops.InterleaveDataset(self._dataset, map_func, cycle_length,
                                      block_length))

  @deprecation.deprecated(None, "Use `ds.apply(tf.contrib.data.unbatch())`.")
  def unbatch(self):
    """Deprecated: Use `Dataset.apply(tf.contrib.data.unbatch()`."""

    return self.apply(batching.unbatch())

  def filter(self, predicate):
    """Filters this dataset according to `predicate`.

    Args:
      predicate: A function mapping a nested structure of tensors (having shapes
        and types defined by `self.output_shapes` and `self.output_types`) to a
        scalar `tf.bool` tensor.

    Returns:
      A `Dataset`.
    """
    return Dataset(dataset_ops.FilterDataset(self._dataset, predicate))

  def apply(self, transformation_func):
    """Apply a transformation function to this dataset.

    `apply` enables chaining of custom `Dataset` transformations, which are
    represented as functions that take one `Dataset` argument and return a
    transformed `Dataset`.

    For example:

    ```
    dataset = (dataset.map(lambda x: x ** 2)
               .(group_by_window(key_func, reduce_func, window_size))
               .map(lambda x: x ** 3))
    ```

    Args:
      transformation_func: A function that takes one `Dataset` argument and
        returns a `Dataset`.

    Returns:
      The `Dataset` returned by applying `transformation_func` to this dataset.
    """
    dataset = transformation_func(self)
    if not isinstance(dataset, dataset_ops.Dataset):
      raise TypeError("`transformation_func` must return a Dataset.")
    return Dataset(dataset)


def get_single_element(dataset):
  """Returns the single element in `dataset` as a nested structure of tensors.

  This function enables you to use a @{tf.data.Dataset} in a stateless
  "tensor-in tensor-out" expression, without creating a @{tf.data.Iterator}.
  This can be useful when your preprocessing transformations are expressed
  as a `Dataset`, and you want to use the transformation at serving time.
  For example:

  ```python
  input_batch = tf.placeholder(tf.string, shape=[BATCH_SIZE])

  def preprocessing_fn(input_str):
    # ...
    return image, label

  dataset = (tf.data.Dataset.from_tensor_slices(input_batch)
             .map(preprocessing_fn, num_parallel_calls=BATCH_SIZE)
             .batch(BATCH_SIZE))

  image_batch, label_batch = tf.contrib.data.get_single_element(dataset)
  ```

  Args:
    dataset: A @{tf.data.Dataset} object containing a single element.

  Returns:
    A nested structure of @{tf.Tensor} objects, corresponding to the single
    element of `dataset`.

  Raises:
    TypeError: if `dataset` is not a `tf.data.Dataset` object.
    InvalidArgumentError (at runtime): if `dataset` does not contain exactly
      one element.
  """
  if not isinstance(dataset, dataset_ops.Dataset):
    raise TypeError("`dataset` must be a `tf.data.Dataset` object.")
  return nest.pack_sequence_as(
      dataset.output_types,
      gen_dataset_ops.dataset_to_single_element(
          dataset._as_variant_tensor(),  # pylint: disable=protected-access
          output_types=nest.flatten(dataset.output_types),
          output_shapes=nest.flatten(dataset.output_shapes)))
