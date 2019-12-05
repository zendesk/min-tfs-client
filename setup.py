from setuptools import setup, find_namespace_packages


setup(
    name="min_tf_client",
    version="1.14",
    description="A minified Tensor Serving Client for Python",
    package_dir={"": "tensor_serving_client"},
    packages=find_namespace_packages(where="tensor_serving_client"),
)
