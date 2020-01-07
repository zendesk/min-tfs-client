import textwrap

import numpy as np
from google.protobuf import text_format

from min_tfs_client.tensors import (coerce_to_bytes, extract_shape, ndarray_to_tensor_proto,
                                   tensor_proto_to_ndarray, write_values_to_tensor_proto)
from min_tfs_client.types import DataType
from tensorflow.core.framework import types_pb2
from tensorflow.core.framework.tensor_pb2 import TensorProto
from tensorflow.core.framework.tensor_shape_pb2 import TensorShapeProto


def _assert_proto_equal(proto, expected: str) -> None:
    expected_string = textwrap.dedent(expected)
    with_leading_newline_stripped = (
        expected_string[1:] if expected_string[0] == "\n" else expected_string
    )

    proto_string = text_format.MessageToString(proto)

    assert proto_string == with_leading_newline_stripped


def test_coerce_to_bytes_coerces_a_string():
    text = "Ceci n'est pas une string"

    result = coerce_to_bytes(text)

    assert type(result) == bytes


def test_coerce_to_bytes_on_bytes():
    text = b"Ceci n'est pas une string"

    result = coerce_to_bytes(text)

    assert result == text
    assert type(result) == bytes


def test_write_values_to_tensor_proto_on_strings():
    dtype = DataType(types_pb2.DT_STRING)
    array = np.array(["Ceci", "n'est", "pas", "une", "string"])
    tensor_proto = TensorProto(
        dtype=dtype.enum, tensor_shape=TensorShapeProto(dim=[TensorShapeProto.Dim(size=len(array))])
    )

    write_values_to_tensor_proto(tensor_proto, array.ravel(), dtype)

    assert tensor_proto.string_val == [b"Ceci", b"n'est", b"pas", b"une", b"string"]


def test_write_values_to_tensor_proto_on_numerics():
    dtype = DataType(types_pb2.DT_FLOAT)
    array = np.array([0.314, 0.159, 0.268])
    tensor_proto = TensorProto(
        dtype=dtype.enum, tensor_shape=TensorShapeProto(dim=[TensorShapeProto.Dim(size=len(array))])
    )

    write_values_to_tensor_proto(tensor_proto, array.ravel(), dtype)

    np.testing.assert_almost_equal(tensor_proto.float_val, [0.314, 0.159, 0.268])


def test_ndarray_to_tensor_proto():
    array = np.array([0.314, 0.159, 0.268, 0.535])
    expected = """
        dtype: DT_DOUBLE
        tensor_shape {
          dim {
            size: 4
          }
        }
        double_val: 0.314
        double_val: 0.159
        double_val: 0.268
        double_val: 0.535
    """

    result = ndarray_to_tensor_proto(array)

    _assert_proto_equal(result, expected)


def test_extract_shape():
    shape = (3, 1, 4)
    tensor_proto = TensorProto(
        dtype=types_pb2.DT_FLOAT,
        tensor_shape=TensorShapeProto(dim=[TensorShapeProto.Dim(size=n) for n in shape]),
    )

    result = extract_shape(tensor_proto)

    assert result == (3, 1, 4)


def test_tensor_proto_to_ndarray():
    dtype = DataType(types_pb2.DT_FLOAT)
    array = np.array([0.314, 0.159, 0.268])
    tensor_proto = TensorProto(
        dtype=dtype.enum, tensor_shape=TensorShapeProto(dim=[TensorShapeProto.Dim(size=len(array))])
    )
    write_values_to_tensor_proto(tensor_proto, array.ravel(), dtype)

    result = tensor_proto_to_ndarray(tensor_proto)

    np.testing.assert_almost_equal(result, np.array([0.314, 0.159, 0.268]))


def test_ndarray_to_tensor_proto_inverse():
    array = np.array([0.314, 0.159, 0.268, 0.535])

    tensor_proto = ndarray_to_tensor_proto(array)
    result = tensor_proto_to_ndarray(tensor_proto)

    np.testing.assert_almost_equal(result, array)
