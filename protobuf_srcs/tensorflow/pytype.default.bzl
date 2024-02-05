"""Default (OSS) build versions of Python pytype rules."""

load("//tensorflow:tensorflow.bzl", _py_test = "py_test")

# Placeholder to use until bazel supports pytype_library.
def pytype_library(name, pytype_deps = [], pytype_srcs = [], **kwargs):
    _ = (pytype_deps, pytype_srcs)  # @unused
    native.py_library(name = name, **kwargs)

# Placeholder to use until bazel supports pytype_strict_binary.
def pytype_strict_binary(name, **kwargs):
    native.py_binary(name = name, **kwargs)

# Placeholder to use until bazel supports pytype_strict_library.
def pytype_strict_library(name, **kwargs):
    native.py_library(name = name, **kwargs)

# Placeholder to use until bazel supports pytype_strict_contrib_test.
def pytype_strict_contrib_test(name, **kwargs):
    _py_test(name = name, **kwargs)
