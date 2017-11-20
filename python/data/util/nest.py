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

# TODO(shivaniagrawal): Merge with core nest
"""## Functions for working with arbitrarily nested sequences of elements.

NOTE(mrry): This fork of the `tensorflow.python.util.nest` module
makes two changes:

1. It adds support for dictionaries as a level of nesting in nested structures.
2. It removes support for lists as a level of nesting in nested structures.

The motivation for this change is twofold:

1. Many input-processing functions (e.g. `tf.parse_example()`) return
   dictionaries, and we would like to support them natively in datasets.
2. It seems more natural for lists to be treated (e.g. in Dataset constructors)
   as tensors, rather than lists of (lists of...) tensors.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import collections as _collections

import six as _six

from tensorflow.python.util.all_util import remove_undocumented


def _sorted(dict_):
  """Returns a sorted list of the dict keys, with error if keys not sortable."""
  try:
    return sorted(_six.iterkeys(dict_))
  except TypeError:
    raise TypeError("nest only supports dicts with sortable keys.")


def _sequence_like(instance, args):
  """Converts the sequence `args` to the same type as `instance`.

  Args:
    instance: an instance of `tuple`, `list`, or a `namedtuple` class.
    args: elements to be converted to a sequence.

  Returns:
    `args` with the type of `instance`.
  """
  if isinstance(instance, dict):
    # Pack dictionaries in a deterministic order by sorting the keys.
    # Notice this means that we ignore the original order of `OrderedDict`
    # instances. This is intentional, to avoid potential bugs caused by mixing
    # ordered and plain dicts (e.g., flattening a dict but using a
    # corresponding `OrderedDict` to pack it back).
    result = dict(zip(_sorted(instance), args))
    return type(instance)((key, result[key]) for key in _six.iterkeys(instance))
  elif (isinstance(instance, tuple) and
        hasattr(instance, "_fields") and
        isinstance(instance._fields, _collections.Sequence) and
        all(isinstance(f, _six.string_types) for f in instance._fields)):
    # This is a namedtuple
    return type(instance)(*args)
  else:
    # Not a namedtuple
    return type(instance)(args)


def _yield_value(iterable):
  if isinstance(iterable, dict):
    # Iterate through dictionaries in a deterministic order by sorting the
    # keys. Notice this means that we ignore the original order of `OrderedDict`
    # instances. This is intentional, to avoid potential bugs caused by mixing
    # ordered and plain dicts (e.g., flattening a dict but using a
    # corresponding `OrderedDict` to pack it back).
    for key in _sorted(iterable):
      yield iterable[key]
  else:
    for value in iterable:
      yield value


def _yield_flat_nest(nest):
  for n in _yield_value(nest):
    if is_sequence(n):
      for ni in _yield_flat_nest(n):
        yield ni
    else:
      yield n


def is_sequence(seq):
  """Returns a true if `seq` is a Sequence or dict (except strings/lists).

  NOTE(mrry): This differs from `tensorflow.python.util.nest.is_sequence()`,
  which *does* treat a Python list as a sequence. For ergonomic
  reasons, `tf.data` users would prefer to treat lists as
  implict `tf.Tensor` objects, and dicts as (nested) sequences.

  Args:
    seq: an input sequence.

  Returns:
    True if the sequence is a not a string or list and is a
    collections.Sequence.
  """
  return (isinstance(seq, (_collections.Sequence, dict))
          and not isinstance(seq, (list, _six.string_types)))


def flatten(nest):
  """Returns a flat sequence from a given nested structure.

  If `nest` is not a sequence, this returns a single-element list: `[nest]`.

  Args:
    nest: an arbitrarily nested structure or a scalar object.
      Note, numpy arrays are considered scalars.

  Returns:
    A Python list, the flattened version of the input.
  """
  return list(_yield_flat_nest(nest)) if is_sequence(nest) else [nest]


def _recursive_assert_same_structure(nest1, nest2, check_types):
  is_sequence_nest1 = is_sequence(nest1)
  if is_sequence_nest1 != is_sequence(nest2):
    raise ValueError(
        "The two structures don't have the same nested structure. "
        "First structure: %s, second structure: %s." % (nest1, nest2))

  if is_sequence_nest1:
    type_nest1 = type(nest1)
    type_nest2 = type(nest2)
    if check_types and type_nest1 != type_nest2:
      raise TypeError(
          "The two structures don't have the same sequence type. First "
          "structure has type %s, while second structure has type %s."
          % (type_nest1, type_nest2))

    for n1, n2 in zip(_yield_value(nest1), _yield_value(nest2)):
      _recursive_assert_same_structure(n1, n2, check_types)


def assert_same_structure(nest1, nest2, check_types=True):
  """Asserts that two structures are nested in the same way.

  Args:
    nest1: an arbitrarily nested structure.
    nest2: an arbitrarily nested structure.
    check_types: if `True` (default) types of sequences are checked as
      well. If set to `False`, for example a list and a tuple of objects will
      look same if they have the same size.

  Raises:
    ValueError: If the two structures do not have the same number of elements or
      if the two structures are not nested in the same way.
    TypeError: If the two structures differ in the type of sequence in any of
      their substructures. Only possible if `check_types` is `True`.
  """
  len_nest1 = len(flatten(nest1)) if is_sequence(nest1) else 1
  len_nest2 = len(flatten(nest2)) if is_sequence(nest2) else 1
  if len_nest1 != len_nest2:
    raise ValueError("The two structures don't have the same number of "
                     "elements. First structure: %s, second structure: %s."
                     % (nest1, nest2))
  _recursive_assert_same_structure(nest1, nest2, check_types)


def _packed_nest_with_indices(structure, flat, index):
  """Helper function for pack_nest_as.

  Args:
    structure: Substructure (tuple of elements and/or tuples) to mimic
    flat: Flattened values to output substructure for.
    index: Index at which to start reading from flat.

  Returns:
    The tuple (new_index, child), where:
      * new_index - the updated index into `flat` having processed `structure`.
      * packed - the subset of `flat` corresponding to `structure`,
                 having started at `index`, and packed into the same nested
                 format.

  Raises:
    ValueError: if `structure` contains more elements than `flat`
      (assuming indexing starts from `index`).
  """
  packed = []
  for s in _yield_value(structure):
    if is_sequence(s):
      new_index, child = _packed_nest_with_indices(s, flat, index)
      packed.append(_sequence_like(s, child))
      index = new_index
    else:
      packed.append(flat[index])
      index += 1
  return index, packed


def pack_sequence_as(structure, flat_sequence):
  """Returns a given flattened sequence packed into a nest.

  If `structure` is a scalar, `flat_sequence` must be a single-element list;
  in this case the return value is `flat_sequence[0]`.

  Args:
    structure: tuple or list constructed of scalars and/or other tuples/lists,
      or a scalar.  Note: numpy arrays are considered scalars.
    flat_sequence: flat sequence to pack.

  Returns:
    packed: `flat_sequence` converted to have the same recursive structure as
      `structure`.

  Raises:
    ValueError: If nest and structure have different element counts.
  """
  if not (is_sequence(flat_sequence) or isinstance(flat_sequence, list)):
    raise TypeError("flat_sequence must be a sequence")

  if not is_sequence(structure):
    if len(flat_sequence) != 1:
      raise ValueError("Structure is a scalar but len(flat_sequence) == %d > 1"
                       % len(flat_sequence))
    return flat_sequence[0]

  flat_structure = flatten(structure)
  if len(flat_structure) != len(flat_sequence):
    raise ValueError(
        "Could not pack sequence. Structure had %d elements, but flat_sequence "
        "had %d elements.  Structure: %s, flat_sequence: %s."
        % (len(flat_structure), len(flat_sequence), structure, flat_sequence))

  _, packed = _packed_nest_with_indices(structure, flat_sequence, 0)
  return _sequence_like(structure, packed)


def map_structure(func, *structure, **check_types_dict):
  """Applies `func` to each entry in `structure` and returns a new structure.

  Applies `func(x[0], x[1], ...)` where x[i] is an entry in
  `structure[i]`.  All structures in `structure` must have the same arity,
  and the return value will contain the results in the same structure.

  Args:
    func: A callable that acceps as many arguments are there are structures.
    *structure: scalar, or tuple or list of constructed scalars and/or other
      tuples/lists, or scalars.  Note: numpy arrays are considered scalars.
    **check_types_dict: only valid keyword argument is `check_types`. If set to
      `True` (default) the types of iterables within the structures have to be
      same (e.g. `map_structure(func, [1], (1,))` raises a `TypeError`
      exception). To allow this set this argument to `False`.

  Returns:
    A new structure with the same arity as `structure`, whose values correspond
    to `func(x[0], x[1], ...)` where `x[i]` is a value in the corresponding
    location in `structure[i]`. If there are different sequence types and
    `check_types` is `False` the sequence types of the first structure will be
    used.

  Raises:
    TypeError: If `func` is not callable or if the structures do not match
      each other by depth tree.
    ValueError: If no structure is provided or if the structures do not match
      each other by type.
    ValueError: If wrong keyword arguments are provided.
  """
  if not callable(func):
    raise TypeError("func must be callable, got: %s" % func)

  if not structure:
    raise ValueError("Must provide at least one structure")

  if check_types_dict:
    if "check_types" not in check_types_dict or len(check_types_dict) > 1:
      raise ValueError("Only valid keyword argument is check_types")
    check_types = check_types_dict["check_types"]
  else:
    check_types = True

  for other in structure[1:]:
    assert_same_structure(structure[0], other, check_types=check_types)

  flat_structure = [flatten(s) for s in structure]
  entries = zip(*flat_structure)

  return pack_sequence_as(
      structure[0], [func(*x) for x in entries])


def _yield_flat_up_to(shallow_tree, input_tree):
  """Yields elements `input_tree` partially flattened up to `shallow_tree`."""
  if is_sequence(shallow_tree):
    for shallow_branch, input_branch in zip(_yield_value(shallow_tree),
                                            _yield_value(input_tree)):
      for input_leaf in _yield_flat_up_to(shallow_branch, input_branch):
        yield input_leaf
  else:
    yield input_tree


def assert_shallow_structure(shallow_tree, input_tree, check_types=True):
  """Asserts that `shallow_tree` is a shallow structure of `input_tree`.

  That is, this function tests if the `input_tree` structure can be created from
  the `shallow_tree` structure by replacing its leaf nodes with deeper
  tree structures.

  Examples:

  The following code will raise an exception:
  ```python
    shallow_tree = ["a", "b"]
    input_tree = ["c", ["d", "e"], "f"]
    assert_shallow_structure(shallow_tree, input_tree)
  ```

  The following code will not raise an exception:
  ```python
    shallow_tree = ["a", "b"]
    input_tree = ["c", ["d", "e"]]
    assert_shallow_structure(shallow_tree, input_tree)
  ```

  Args:
    shallow_tree: an arbitrarily nested structure.
    input_tree: an arbitrarily nested structure.
    check_types: if `True` (default) the sequence types of `shallow_tree` and
      `input_tree` have to be the same.

  Raises:
    TypeError: If `shallow_tree` is a sequence but `input_tree` is not.
    TypeError: If the sequence types of `shallow_tree` are different from
      `input_tree`. Only raised if `check_types` is `True`.
    ValueError: If the sequence lengths of `shallow_tree` are different from
      `input_tree`.
  """
  if is_sequence(shallow_tree):
    if not is_sequence(input_tree):
      raise TypeError(
          "If shallow structure is a sequence, input must also be a sequence. "
          "Input has type: %s." % type(input_tree))

    if check_types and not isinstance(input_tree, type(shallow_tree)):
      raise TypeError(
          "The two structures don't have the same sequence type. Input "
          "structure has type %s, while shallow structure has type %s."
          % (type(input_tree), type(shallow_tree)))

    if len(input_tree) != len(shallow_tree):
      raise ValueError(
          "The two structures don't have the same sequence length. Input "
          "structure has length %s, while shallow structure has length %s."
          % (len(input_tree), len(shallow_tree)))

    if check_types and isinstance(shallow_tree, dict):
      if set(input_tree) != set(shallow_tree):
        raise ValueError(
          "The two structures don't have the same keys. Input "
          "structure has keys %s, while shallow structure has keys %s."
          % (list(_six.iterkeys(input_tree)),
             list(_six.iterkeys(shallow_tree))))
      input_tree = list(_six.iteritems(input_tree))
      shallow_tree = list(_six.iteritems(shallow_tree))

    for shallow_branch, input_branch in zip(shallow_tree, input_tree):
      assert_shallow_structure(shallow_branch, input_branch,
                               check_types=check_types)


def flatten_up_to(shallow_tree, input_tree):
  """Flattens `input_tree` up to `shallow_tree`.

  Any further depth in structure in `input_tree` is retained as elements in the
  partially flatten output.

  If `shallow_tree` and `input_tree` are not sequences, this returns a
  single-element list: `[input_tree]`.

  Use Case:

  Sometimes we may wish to partially flatten a nested sequence, retaining some
  of the nested structure. We achieve this by specifying a shallow structure,
  `shallow_tree`, we wish to flatten up to.

  The input, `input_tree`, can be thought of as having the same structure as
  `shallow_tree`, but with leaf nodes that are themselves tree structures.

  Examples:

  ```python
  input_tree = [[[2, 2], [3, 3]], [[4, 9], [5, 5]]]
  shallow_tree = [[True, True], [False, True]]

  flattened_input_tree = flatten_up_to(shallow_tree, input_tree)
  flattened_shallow_tree = flatten_up_to(shallow_tree, shallow_tree)

  # Output is:
  # [[2, 2], [3, 3], [4, 9], [5, 5]]
  # [True, True, False, True]
  ```

  ```python
  input_tree = [[('a', 1), [('b', 2), [('c', 3), [('d', 4)]]]]]
  shallow_tree = [['level_1', ['level_2', ['level_3', ['level_4']]]]]

  input_tree_flattened_as_shallow_tree = flatten_up_to(shallow_tree, input_tree)
  input_tree_flattened = flatten(input_tree)

  # Output is:
  # [('a', 1), ('b', 2), ('c', 3), ('d', 4)]
  # ['a', 1, 'b', 2, 'c', 3, 'd', 4]
  ```

  Non-Sequence Edge Cases:

  ```python
  flatten_up_to(0, 0)  # Output: [0]
  flatten_up_to(0, [0, 1, 2])  # Output: [[0, 1, 2]]
  flatten_up_to([0, 1, 2], 0)  # Output: TypeError
  flatten_up_to([0, 1, 2], [0, 1, 2])  # Output: [0, 1, 2]
  ```

  Args:
    shallow_tree: a possibly pruned structure of input_tree.
    input_tree: an arbitrarily nested structure or a scalar object.
      Note, numpy arrays are considered scalars.

  Returns:
    A Python list, the partially flattened version of `input_tree` according to
    the structure of `shallow_tree`.

  Raises:
    TypeError: If `shallow_tree` is a sequence but `input_tree` is not.
    TypeError: If the sequence types of `shallow_tree` are different from
      `input_tree`.
    ValueError: If the sequence lengths of `shallow_tree` are different from
      `input_tree`.
  """
  assert_shallow_structure(shallow_tree, input_tree)
  return list(_yield_flat_up_to(shallow_tree, input_tree))


def map_structure_up_to(shallow_tree, func, *inputs):
  """Applies a function or op to a number of partially flattened inputs.

  The `inputs` are flattened up to `shallow_tree` before being mapped.

  Use Case:

  Sometimes we wish to apply a function to a partially flattened
  sequence (for example when the function itself takes sequence inputs). We
  achieve this by specifying a shallow structure, `shallow_tree` we wish to
  flatten up to.

  The `inputs`, can be thought of as having the same structure as
  `shallow_tree`, but with leaf nodes that are themselves tree structures.

  This function therefore will return something with the same base structure as
  `shallow_tree`.

  Examples:

  ```python
  ab_tuple = collections.namedtuple("ab_tuple", "a, b")
  op_tuple = collections.namedtuple("op_tuple", "add, mul")
  inp_val = ab_tuple(a=2, b=3)
  inp_ops = ab_tuple(a=op_tuple(add=1, mul=2), b=op_tuple(add=2, mul=3))
  out = map_structure_up_to(inp_val, lambda val, ops: (val + ops.add) * ops.mul,
                            inp_val, inp_ops)

  # Output is: ab_tuple(a=6, b=15)
  ```

  ```python
  data_list = [[2, 4, 6, 8], [[1, 3, 5, 7, 9], [3, 5, 7]]]
  name_list = ['evens', ['odds', 'primes']]
  out = map_structure_up_to(
      name_list,
      lambda name, sec: "first_{}_{}".format(len(sec), name),
      name_list, data_list)

  # Output is: ['first_4_evens', ['first_5_odds', 'first_3_primes']]
  ```

  Args:
    shallow_tree: a shallow tree, common to all the inputs.
    func: callable which will be applied to each input individually.
    *inputs: arbitrarily nested combination of objects that are compatible with
        shallow_tree. The function `func` is applied to corresponding
        partially flattened elements of each input, so the function must support
        arity of `len(inputs)`.

  Raises:
    TypeError: If `shallow_tree` is a sequence but `input_tree` is not.
    TypeError: If the sequence types of `shallow_tree` are different from
      `input_tree`.
    ValueError: If the sequence lengths of `shallow_tree` are different from
      `input_tree`.

  Returns:
    result of repeatedly applying `func`, with same structure as
    `shallow_tree`.
  """
  if not inputs:
    raise ValueError("Cannot map over no sequences")
  for input_tree in inputs:
    assert_shallow_structure(shallow_tree, input_tree)

  # Flatten each input separately, apply the function to corresponding elements,
  # then repack based on the structure of the first input.
  all_flattened_up_to = [flatten_up_to(shallow_tree, input_tree)
                         for input_tree in inputs]

  results = [func(*tensors) for tensors in zip(*all_flattened_up_to)]
  return pack_sequence_as(structure=shallow_tree, flat_sequence=results)


_allowed_symbols = [
    "assert_same_structure",
    "is_sequence",
    "flatten",
    "pack_sequence_as",
    "map_structure",
    "assert_shallow_structure",
    "flatten_up_to",
    "map_structure_up_to",
]

remove_undocumented(__name__, _allowed_symbols)
