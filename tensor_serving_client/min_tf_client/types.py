import numpy as np
from tensorflow.core.framework import types_pb2
from typing import NamedTuple


class TFType(NamedTuple):
    TFDType: str
    TensorProtoField: str


class dtypes:
    NP_TO_TF_MAPPING = {
        "float16": TFType(TFDType="DT_HALF", TensorProtoField="half_val"),
        "float32": TFType(TFDType="DT_FLOAT", TensorProtoField="float_val"),
        "float64": TFType(TFDType="DT_DOUBLE", TensorProtoField="double_val"),
        "int32": TFType(TFDType="DT_INT32", TensorProtoField="some_val"),
        "uint8": TFType(TFDType="DT_UINT8", TensorProtoField="some_val"),
        "uint16": TFType(TFDType="DT_UINT16", TensorProtoField="some_val"),
        "uint32": TFType(TFDType="DT_UINT32", TensorProtoField="some_val"),
        "uint64": TFType(TFDType="DT_UINT64", TensorProtoField="some_val"),
        "int16": TFType(TFDType="DT_INT16", TensorProtoField="some_val"),
        "int8": TFType(TFDType="DT_INT8", TensorProtoField="some_val"),
        "object": TFType(TFDType="DT_STRING", TensorProtoField="string_val"),
        "complex64": TFType(TFDType="DT_COMPLEX64", TensorProtoField="some_val"),
        "complex128": TFType(TFDType="DT_COMPLEX128", TensorProtoField="some_val"),
        "int64": TFType(TFDType="DT_INT64", TensorProtoField="some_val"),
        "bool": TFType(TFDType="DT_BOOL", TensorProtoField="some_val"),
    }


# NOTE: These type mappings are based on:
# https://github.com/tensorflow/tensorflow/blob/r1.14/tensorflow/python/framework/dtypes.py#L573
_TF_TO_NP = {
    "float16": "DT_HALF",
    "float32": "DT_FLOAT",
    "float64": "DT_DOUBLE",
    "int32": "DT_INT32",
    "uint8": "DT_UINT8",
    "uint16": "DT_UINT16",
    "uint32": "DT_UINT32",
    "uint64": "DT_UINT64",
    "int16": "DT_INT16",
    "int8": "DT_INT8",
    "object": "DT_STRING",
    "complex64": "DT_COMPLEX64",
    "complex128": "DT_COMPLEX128",
    "int64": "DT_INT64",
    "bool": "DT_BOOL",
}

_NP_TO_TF = {v: k for k, v in _TF_TO_NP.items()}

_TF_ENUM_TO_TF = {getattr(types_pb2, t): t for t in _TF_TO_NP.keys()}


def numpy_dtype_to_tf_type_enum(dtype: np.dtype) -> int:
    return getattr(types_pb2, _NP_TO_TF.get(dtype.name, "DT_INVALID"))


def tf_type_enum_to_numpy_dtype(tf_type_enum: int) -> np.dtype:
    return _TF_TO_NP.get(_TF_ENUM_TO_TF[tf_type_enum], "object")
