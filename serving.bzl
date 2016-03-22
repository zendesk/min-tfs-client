load("@tf//google/protobuf:protobuf.bzl", "cc_proto_library")
load("@tf//google/protobuf:protobuf.bzl", "py_proto_library")

def serving_proto_library(name, srcs=[], has_services=False,
                          deps=[], visibility=None, testonly=0,
                          cc_grpc_version = None,
                          cc_api_version=2, go_api_version=2,
                          java_api_version=2,
                          py_api_version=2):
  native.filegroup(name=name + "_proto_srcs",
                   srcs=srcs,
                   testonly=testonly,)

  use_grpc_plugin = None
  if cc_grpc_version:
    use_grpc_plugin = True
  cc_proto_library(name=name,
                   srcs=srcs,
                   deps=deps,
                   cc_libs = ["@tf//google/protobuf:protobuf"],
                   protoc="@tf//google/protobuf:protoc",
                   default_runtime="@tf//google/protobuf:protobuf",
                   use_grpc_plugin=use_grpc_plugin,
                   testonly=testonly,
                   visibility=visibility,)

def serving_proto_library_py(name, srcs=[], deps=[], visibility=None, testonly=0):
  py_proto_library(name=name,
                   srcs=srcs,
                   srcs_version = "PY2AND3",
                   deps=deps,
                   default_runtime="@tf//google/protobuf:protobuf_python",
                   protoc="@tf//google/protobuf:protoc",
                   visibility=visibility,
                   testonly=testonly,)
