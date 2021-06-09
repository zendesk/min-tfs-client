import os
from contextlib import contextmanager
from distutils.cmd import Command
from pathlib import Path
from shutil import copy2
from subprocess import check_output
from typing import Any, List

from setuptools import find_namespace_packages, setup
from setuptools.command.build_py import build_py

ROOT_DIR = Path(os.path.dirname(os.path.realpath(__file__)))
PROTOBUF_INCLUDE_PATH = ROOT_DIR / "protobuf_srcs/"
OUTPUT_PATH = ROOT_DIR / "tensor_serving_client/"
PROTO_PATHS = [str(p) for p in PROTOBUF_INCLUDE_PATH.rglob("*.proto")]


@contextmanager
def cd(newdir):
    prevdir = os.getcwd()
    os.chdir(os.path.expanduser(newdir))
    try:
        yield
    finally:
        os.chdir(prevdir)


class CompileProtobufs(Command):
    description = """
    Compile .proto files in Tensorflow and Tensorflow Serving into python
    libraries.
    """
    user_options: List[Any] = []

    def initialize_options(self):
        ...

    def finalize_options(self):
        ...

    def run(self) -> None:
        pb_compile_command = [
            "protoc",
            f"-I={str(PROTOBUF_INCLUDE_PATH)}",
            f"--python_out={str(OUTPUT_PATH)}",
        ]
        with cd(str(PROTOBUF_INCLUDE_PATH)):
            for proto_path in PROTO_PATHS:
                check_output(pb_compile_command + [proto_path])


class CopyGRPCStubs(Command):
    description = """
    Copy GRPC Stubs from Tensorflow Serving APIs to the minified Tensorflow Serving APIs library
    """
    user_options: List[Any] = []

    def initialize_options(self):
        ...

    def finalize_options(self):
        ...

    def run(self):
        predict_service_stub = (
            PROTOBUF_INCLUDE_PATH / "tensorflow_serving/apis/prediction_service_pb2_grpc.py"
        )
        model_service_stub = (
            PROTOBUF_INCLUDE_PATH / "tensorflow_serving/apis/model_service_pb2_grpc.py"
        )
        destination = ROOT_DIR / "tensor_serving_client/tensorflow_serving/apis/"
        copy2(predict_service_stub, destination)
        copy2(model_service_stub, destination)


class BuildPyCommand(build_py):
    """Custom build command."""

    def run(self):
        self.run_command("compile_pb")
        self.run_command("copy_grpc")
        build_py.run(self)


with open("README.md", "r") as fh:
    long_description = fh.read()


setup(
    name="min_tfs_client",
    version="1.0.3+whl",
    description="A minified Tensor Serving Client for Python",
    long_description=long_description,
    long_description_content_type="text/markdown",
    cmdclass={
        "compile_pb": CompileProtobufs,
        "copy_grpc": CopyGRPCStubs,
        "build_py": BuildPyCommand,
    },
    package_dir={"": "tensor_serving_client"},
    packages=find_namespace_packages(where="tensor_serving_client"),
    install_requires=["grpcio>=1.21", "protobuf>=3.8", "numpy>=1.16.4"],
    tests_require=["pytest"],
    extras_require={"dev": ["black==19.10b0", "flake8", "mypy", "pytest"]},
)
