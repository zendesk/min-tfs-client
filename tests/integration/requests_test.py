import json

import numpy as np
from google.protobuf.json_format import MessageToJson
from numpy.testing import assert_array_almost_equal, assert_array_equal

from min_tfs_client.requests import TensorServingClient
from min_tfs_client.tensors import tensor_proto_to_ndarray
from pytest import fixture


@fixture(scope="function")
def unsecured_ts_client() -> TensorServingClient:
    return TensorServingClient(host="127.0.0.1", port=4080, credentials=None)


def test_predict_request(unsecured_ts_client):
    client = unsecured_ts_client
    response = client.predict_request(
        model_name="default",
        model_version=1,
        input_dict={
            "string_input": np.array(["hello world"]),
            "float_input": np.array([0.1], dtype=np.float32),
            "int_input": np.array([2], dtype=np.int64),
        },
    )
    assert_array_almost_equal(
        tensor_proto_to_ndarray(response.outputs["float_output"]), np.array([0.1], dtype=np.float32)
    )
    assert_array_equal(
        tensor_proto_to_ndarray(response.outputs["int_output"]), np.array([2], dtype=np.int64)
    )
    assert_array_equal(
        tensor_proto_to_ndarray(response.outputs["string_output"]), np.array(["hello world"])
    )


def test_model_status_request(unsecured_ts_client):
    client = unsecured_ts_client
    response = client.model_status_request(model_name="default")
    response_dict = json.loads(MessageToJson(response))

    assert "model_version_status" in response_dict
    assert len(response_dict["model_version_status"]) == 1
    assert response_dict["model_version_status"][0] == {
        "version": "1",
        "state": "AVAILABLE",
        "status": {},
    }

def test_model_metadata_request(unsecured_ts_client):
    client = unsecured_ts_client
    response = client.model_metadata_request(model_name="default")
    response_dict = json.loads(MessageToJson(response))    

    assert "modelSpec" in response_dict
    assert response_dict["modelSpec"] == {
        "name": "default",
        "version": "1"
    }
    assert "metadata" in response_dict
    assert list(response_dict["metadata"].keys()) == ["signature_def"]
    assert "signatureDef" in response_dict["metadata"]["signature_def"]
    sig_def = response_dict["metadata"]["signature_def"]["signatureDef"]
    assert "inputs" in sig_def
    assert "serving_default" in sig_def
