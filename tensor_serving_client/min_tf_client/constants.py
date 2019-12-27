from typing import NamedTuple

import numpy as np

from tensorflow.core.framework import types_pb2


class TFType(NamedTuple):
    TFDType: str
    TensorProtoField: str


NP_TO_TF_MAPPING = {
    np.float16: TFType(TFDType="DT_HALF", TensorProtoField="half_val"),
    np.float32: TFType(TFDType="DT_FLOAT", TensorProtoField="float_val"),
    np.float64: TFType(TFDType="DT_DOUBLE", TensorProtoField="double_val"),
    np.int8: TFType(TFDType="DT_INT8", TensorProtoField="int_val"),
    np.int16: TFType(TFDType="DT_INT16", TensorProtoField="int_val"),
    np.int32: TFType(TFDType="DT_INT32", TensorProtoField="int_val"),
    np.int64: TFType(TFDType="DT_INT64", TensorProtoField="int64_val"),
    np.uint8: TFType(TFDType="DT_UINT8", TensorProtoField="int_val"),
    np.uint16: TFType(TFDType="DT_UINT16", TensorProtoField="int_val"),
    np.uint32: TFType(TFDType="DT_UINT32", TensorProtoField="uint32_val"),
    np.uint64: TFType(TFDType="DT_UINT64", TensorProtoField="uint64_val"),
    np.complex64: TFType(TFDType="DT_COMPLEX64", TensorProtoField="scomplex_val"),
    np.complex128: TFType(TFDType="DT_COMPLEX128", TensorProtoField="dcomplex_val"),
    np.str_: TFType(TFDType="DT_STRING", TensorProtoField="string_val"),
    np.bool_: TFType(TFDType="DT_BOOL", TensorProtoField="bool_val"),
}

TF_TO_NP_MAPPING = {v.TFDType: k for k, v in NP_TO_TF_MAPPING.items()}
NP_TO_ENUM_MAPPING = {k: getattr(types_pb2, v.TFDType) for k, v in NP_TO_TF_MAPPING.items()}
ENUM_TO_TF_MAPPING = {v: NP_TO_TF_MAPPING[k].TFDType for k, v in NP_TO_ENUM_MAPPING.items()}

NUMERICAL_TYPES = {
    np.float16,
    np.float32,
    np.float64,
    np.int32,
    np.uint8,
    np.uint16,
    np.uint32,
    np.uint64,
    np.int16,
    np.int8,
    np.complex64,
    np.complex128,
    np.int64,
    np.bool_,
}
