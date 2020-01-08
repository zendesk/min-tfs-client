import numpy as np
from numpy.testing import assert_array_almost_equal, assert_array_equal
from pytest import fixture

from min_tfs_client.requests import TensorServingClient
from min_tfs_client.tensors import tensor_proto_to_ndarray


@fixture(scope="function")
def unsecured_ts_client():
    return TensorServingClient(host="127.0.0.1", port=4080, credentials=None)


def test_request(unsecured_ts_client):
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
