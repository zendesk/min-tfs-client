# Minimal Tensor Serving Python Client
A lightweight python client to communicate with Tensor Serving.

## Description
Communicating with Tensorflow models via Tensor Serving requires [gRPC](https://grpc.io/) and Tensorflow-specific protobufs. The `tensorflow-serving-apis` package on PyPI provides these interfaces, but requires `tensorflow` as a dependency. The Tensorflow python package currently stands at 700 Mb, with much of this space dedicated to libraries and executables required for training, saving, and visualising Tensorflow Models; these libraries are not required at inference time when communicating with Tensorflow Serving.

This package exposes a minimal Tensor Serving client that does not include Tensorflow as a dependency. This reduces the overall package size to < 1 Mb. This is particularly useful when deploying web services via AWS Lambda that need to communicate with Tensorflow Serving, as Lambda carries a size limit on deployments.

## Install from PyPi
This is the quickest way to get started! Just run:

```Bash
pip install min-tfs-client
```

## Installing from source
Installation from source will require the protobuf compiler `protoc` to be installed and available to the command line (e.g. via the `PATH` environment variable). The protobuf compiler can be downloaded from the [protocolbuffers/protobuf](https://github.com/protocolbuffers/protobuf/releases) Github repo. Once `protoc` is installed and available, you can run:

```Bash
git clone https://github.com/zendesk/min-tfs-client.git
cd min-tfs-client
python setup.py compile_pb copy_grpc
pip install .
```

## Development Installation
For dev installation, run `pip install -e .` instead of `pip install .`. Also, you will require `tensorflow-model-server` and `tensorflow` to be installed to run and modify the integration tests. Specifically:

1. `tensorflow` is required to run the model generation script (`tests/integration/fixtures`) that creates a test model for integration testing. It is **not** required to just run the tests.
2. `tensorflow-model-server` is required to serve the model to perform the integration test. The commands that are used to run these tests in Travis are contained in `.travis.yml`.

## Usage
Basic Usage
``` Python
from min_tfs_client.requests import TensorServingClient
from min_tfs_client.tensors import tensor_proto_to_ndarray

client = TensorServingClient(host="127.0.0.1", port=4080, credentials=None)
response = client.predict_request(
    model_name="default",
    model_version=1,
    input_dict={
        # These input keys are model-specific
        "string_input": np.array(["hello world"]),
        "float_input": np.array([0.1], dtype=np.float32),
        "int_input": np.array([2], dtype=np.int64),
    },
)
float_output = tensor_proto_to_ndarray(
    # This output key is model-specific
    response.outputs["float_output"]
)
```

## Running tests

Run all tests with

```Bash
pytest -v tests/
```

Run a single test file with

```Bash
pytest <path_to_test_file>
```

Run unit / integration tests with

```Bash
pytest tests/<unit or integration>
```

## Contribution Guidelines
Improvements are always welcome. Please follow these steps to contribute:
1. Submit a Pull Request with a detailed explanation of changes
2. Receive approval from maintainers
3. Maintainers will merge your changes

## Licence Information
Use of this software is subject to important terms and conditions as set forth in the [LICENSE](LICENCE) file.

The code contained within [protobuf_srcs/tensorflow](protobuf_srcs/tensorflow) is forked from [Tensorflow](https://github.com/tensorflow/tensorflow), and the code contained within [protobuf_srcs/tensorflow_serving](protobuf_srcs/tensorflow_serving) is forked from [Tensorflow Serving](https://github.com/tensorflow/serving). Please refer to the individual source files within `protobuf_srcs` for individual file licence information.