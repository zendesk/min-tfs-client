import numpy as np
import pytest

from min_tfs_client.types import DataType


TEST_TARGETS = [
    (np.float16, "DT_HALF", 19),
    (np.float32, "DT_FLOAT", 1),
    (np.float64, "DT_DOUBLE", 2),
    (np.int8, "DT_INT8", 6),
    (np.int16, "DT_INT16", 5),
    (np.int32, "DT_INT32", 3),
    (np.int64, "DT_INT64", 9),
    (np.uint8, "DT_UINT8", 4),
    (np.uint16, "DT_UINT16", 17),
    (np.uint32, "DT_UINT32", 22),
    (np.uint64, "DT_UINT64", 23),
    (np.complex64, "DT_COMPLEX64", 8),
    (np.complex128, "DT_COMPLEX128", 18),
    (np.str_, "DT_STRING", 7),
    (np.bool_, "DT_BOOL", 10)
]


def verify_datatype_instance(dt_instance: DataType, np_dtype: type, tf_dtype: str, enum: int):
    assert dt_instance.numpy_dtype == np_dtype
    assert dt_instance.tf_dtype == tf_dtype
    assert dt_instance.enum == enum


@pytest.mark.parametrize(
    "np_dtype,tf_dtype,enum",
    TEST_TARGETS,
)
def test_datatypes(np_dtype, tf_dtype, enum):
    actual = {
        "numpy": DataType(np_dtype),
        "tensorflow": DataType(tf_dtype),
        "tensor_proto_enum": DataType(enum)
    }
    for dt_instance in actual.values():
        verify_datatype_instance(dt_instance, np_dtype, tf_dtype, enum)
