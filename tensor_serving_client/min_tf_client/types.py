from typing import Union
import numpy as np

from .constants import (
    ENUM_TO_TF_MAPPING,
    NP_TO_ENUM_MAPPING,
    NP_TO_TF_MAPPING,
    NUMERICAL_TYPES,
    TF_TO_NP_MAPPING
)


class DataType:
    VALID_TYPES = (
        NUMERICAL_TYPES
        .union({np.str_, np.bool_})
    )

    def __init__(self, dtype: Union[type, str, int]):
        self.numpy_dtype = self._get_numpy_dtype(dtype)
        self._validate_dtype(self.numpy_dtype)
        self.is_numeric = self.numpy_dtype in NUMERICAL_TYPES
        self.tf_dtype = NP_TO_TF_MAPPING[self.numpy_dtype].TFDType
        self.enum = NP_TO_ENUM_MAPPING[self.numpy_dtype]
        self.proto_field_name = NP_TO_TF_MAPPING[self.numpy_dtype].TensorProtoField

    def _validate_dtype(self, numpy_dtype: type) -> None:
        if numpy_dtype not in self.VALID_TYPES:
            raise ValueError(
                f"Dtype {str(numpy_dtype.__name__)} is not valid. "
                f"Allowable values: {', '.join([t.__name__ for t in self.VALID_TYPES])}"
            )

    def _get_numpy_dtype(self, dtype: Union[type, str, int]) -> type:
        if isinstance(dtype, type):
            return dtype
        elif isinstance(dtype, str):
            return np.dtype(TF_TO_NP_MAPPING[dtype]).type
        elif isinstance(dtype, int):
            return np.dtype(TF_TO_NP_MAPPING[ENUM_TO_TF_MAPPING[dtype]]).type
        else:
            raise ValueError(f"Expected dtype of types: type, str, or int, got {type(dtype)}")
