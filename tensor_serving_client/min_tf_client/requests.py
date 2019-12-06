from typing import Optional, Dict

import grpc
import numpy as np

from tensorflow_serving.apis.prediction_service_pb2_grpc import PredictionServiceStub
from tensorflow_serving.apis.predict_pb2 import PredictRequest
from .tensors import ndarray_to_tensor_proto


class TensorServingClient:
    def __init__(
        self, host: str, port: int, credentials: Optional[grpc.ssl_channel_credentials] = None,
    ) -> None:
        self._host_address = f"{host}:{port}"
        if credentials:
            self._channel = grpc.secure_channel(self._host_address, credentials)
        else:
            self._channel = grpc.insecure_channel(self._host_address)

    def predict_request(self, model_name: str, input_dict: Dict[str, np.ndarray], timeout=60):
        stub = PredictionServiceStub(self._channel)
        request = PredictRequest()
        request.model_spec.name = model_name
        for k, v in input_dict.items():
            request.inputs[k].CopyFrom(
                ndarray_to_tensor_proto(v)
            )
        return stub.Predict(request, timeout)
