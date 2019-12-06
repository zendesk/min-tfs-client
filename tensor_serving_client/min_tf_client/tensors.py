from .types import numpy_dtype_to_tf_type_enum
from tensorflow.core.framework.tensor_pb2 import TensorProto
from tensorflow.core.framework.tensor_shape_pb2 import TensorShapeProto
import numpy as np


from typing import Iterable


_TFTYPE_TO_PROTO_FIELD = {
    7: "string_val"
}


def load_tensor_proto(tensor_proto: TensorProto, values: Iterable) -> TensorProto:
    proto_field = getattr(tensor_proto, f"{_TFTYPE_TO_PROTO_FIELD[tensor_proto.dtype]}")
    proto_field.extend([v for v in values])
    return tensor_proto


def ndarray_to_tensor_proto(ndarray: np.ndarray) -> TensorProto:
    print(f"hello Dana I am a {numpy_dtype_to_tf_type_enum(ndarray.dtype)}, and was a {ndarray.dtype}")
    proto = TensorProto(
        dtype=numpy_dtype_to_tf_type_enum(ndarray.dtype),
        tensor_shape=TensorShapeProto(
            dim=[TensorShapeProto.Dim(size=d) for d in ndarray.shape]
        )
    )
    proto = load_tensor_proto(proto, ndarray.ravel())
    return proto
