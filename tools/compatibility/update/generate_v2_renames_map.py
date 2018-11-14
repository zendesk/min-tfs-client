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
# pylint: disable=line-too-long
"""Script for updating tensorflow/tools/compatibility/renames_v2.py.

To update renames_v2.py, run:
  bazel build tensorflow/tools/compatibility/update:generate_v2_renames_map
  bazel-bin/tensorflow/tools/compatibility/update/generate_v2_renames_map
"""
# pylint: enable=line-too-long
import sys

import tensorflow as tf

# This import is needed so that TensorFlow python modules are in sys.modules.
from tensorflow import python as tf_python  # pylint: disable=unused-import
from tensorflow.python.lib.io import file_io
from tensorflow.python.platform import app
from tensorflow.python.util import tf_decorator
from tensorflow.python.util import tf_export
from tensorflow.tools.common import public_api
from tensorflow.tools.common import traverse


_OUTPUT_FILE_PATH = 'third_party/tensorflow/tools/compatibility/renames_v2.py'
_FILE_HEADER = """# Copyright 2018 The TensorFlow Authors. All Rights Reserved.
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
# pylint: disable=line-too-long
\"\"\"List of renames to apply when converting from TF 1.0 to TF 2.0.

THIS FILE IS AUTOGENERATED: To update, please run:
  bazel build tensorflow/tools/compatibility/update:generate_v2_renames_map
  bazel-bin/tensorflow/tools/compatibility/update/generate_v2_renames_map
This file should be updated whenever endpoints are deprecated.
\"\"\"
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

"""

_TENSORFLOW_API_ATTR_V1 = (
    tf_export.API_ATTRS_V1[tf_export.TENSORFLOW_API_NAME].names)
_TENSORFLOW_API_ATTR = tf_export.API_ATTRS[tf_export.TENSORFLOW_API_NAME].names
_TENSORFLOW_CONSTANTS_ATTR_V1 = (
    tf_export.API_ATTRS_V1[tf_export.TENSORFLOW_API_NAME].constants)
_TENSORFLOW_CONSTANTS_ATTR = (
    tf_export.API_ATTRS[tf_export.TENSORFLOW_API_NAME].constants)


def get_canonical_name(v2_names, v1_name):
  if v2_names:
    return v2_names[0]
  return 'compat.v1.%s' % v1_name


def collect_constant_renames():
  """Looks for constants that need to be renamed in TF 2.0.

  Returns:
    List of tuples of the form (current name, new name).
  """
  renames = set()
  for module in sys.modules.values():
    if not hasattr(module, _TENSORFLOW_CONSTANTS_ATTR_V1):
      continue
    constants_v1_list = getattr(module, _TENSORFLOW_CONSTANTS_ATTR_V1)
    constants_v2_list = getattr(module, _TENSORFLOW_CONSTANTS_ATTR)

    # _tf_api_constants attribute contains a list of tuples:
    # (api_names_list, constant_name)
    # We want to find API names that are in V1 but not in V2 for the same
    # constant_names.

    # First, we convert constants_v1_list and constants_v2_list to
    # dictionaries for easier lookup.
    constants_v1 = {constant_name: api_names
                    for api_names, constant_name in constants_v1_list}
    constants_v2 = {constant_name: api_names
                    for api_names, constant_name in constants_v2_list}
    # Second, we look for names that are in V1 but not in V2.
    for constant_name, api_names_v1 in constants_v1.items():
      api_names_v2 = constants_v2[constant_name]
      for name in api_names_v1:
        if name not in api_names_v2:
          renames.add((name, get_canonical_name(api_names_v2, name)))
  return renames


def collect_function_renames():
  """Looks for functions/classes that need to be renamed in TF 2.0.

  Returns:
    List of tuples of the form (current name, new name).
  """
  # Set of rename lines to write to output file in the form:
  #   'tf.deprecated_name': 'tf.canonical_name'
  renames = set()
  v2_names = set()  # All op names in TensorFlow 2.0

  def visit(unused_path, unused_parent, children):
    """Visitor that collects rename strings to add to rename_line_set."""
    for child in children:
      _, attr = tf_decorator.unwrap(child[1])
      if not hasattr(attr, '__dict__'):
        continue
      api_names_v1 = attr.__dict__.get(_TENSORFLOW_API_ATTR_V1, [])
      api_names_v2 = attr.__dict__.get(_TENSORFLOW_API_ATTR, [])
      deprecated_api_names = set(api_names_v1) - set(api_names_v2)
      for name in deprecated_api_names:
        renames.add((name, get_canonical_name(api_names_v2, name)))
      for name in api_names_v2:
        v2_names.add(name)

  visitor = public_api.PublicAPIVisitor(visit)
  visitor.do_not_descend_map['tf'].append('contrib')
  visitor.do_not_descend_map['tf.compat'] = ['v1', 'v2']
  traverse.traverse(tf, visitor)

  # It is possible that a different function is exported with the
  # same name. For e.g. when creating a different function to
  # rename arguments. Exclude it from renames in this case.
  renames = {name: new_name for name, new_name in renames.items()
             if name not in v2_names}
  return renames


def get_rename_line(name, canonical_name):
  return '    \'tf.%s\': \'tf.%s\'' % (name, canonical_name)


def update_renames_v2(output_file_path):
  """Writes a Python dictionary mapping deprecated to canonical API names.

  Args:
    output_file_path: File path to write output to. Any existing contents
      would be replaced.
  """
  function_renames = collect_function_renames()
  constant_renames = collect_constant_renames()
  all_renames = function_renames.union(constant_renames)

  # List of rename lines to write to output file in the form:
  #   'tf.deprecated_name': 'tf.canonical_name'
  rename_lines = [
      get_rename_line(name, canonical_name)
      for name, canonical_name in all_renames]
  renames_file_text = '%srenames = {\n%s\n}\n' % (
      _FILE_HEADER, ',\n'.join(sorted(rename_lines)))
  file_io.write_string_to_file(output_file_path, renames_file_text)


def main(unused_argv):
  update_renames_v2(_OUTPUT_FILE_PATH)


if __name__ == '__main__':
  app.run(main=main)
