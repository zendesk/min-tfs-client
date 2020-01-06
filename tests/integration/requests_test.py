from min_tf_client.requests import TensorServingClient
from pytest import fixture
import numpy as np


@fixture(scope="function")
def unsecured_ts_client():
    return TensorServingClient(
        host="127.0.0.1",
        port=4080,
        credentials=None
    )


def test_request(unsecured_ts_client):
    client = unsecured_ts_client
    response = client.predict_request(
        model_name="default",
        model_version=1,
        input_dict={
            "string_input": np.array(["hello world"]),
            "float_input": np.array([0.1], dtype=np.float32),
            "int_input": np.array([2], dtype=np.int64)
        }
    )
    print(response)
    assert response is not None
