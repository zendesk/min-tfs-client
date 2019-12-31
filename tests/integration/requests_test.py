from min_tf_client.requests import TensorServingClient
from pytest import fixture


@fixture(scope="function")
def unsecured_ts_client():
    return TensorServingClient(
        host="127.0.0.1",
        port=4080,
        credentials=None
    )


def test_request(unsecured_ts_client):
    ...
