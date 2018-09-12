/* Copyright 2018 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_SERVING_MODEL_SERVERS_SERVER_H_
#define TENSORFLOW_SERVING_MODEL_SERVERS_SERVER_H_

#include <memory>

#include "grpcpp/server.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/platform/cpu_info.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow_serving/model_servers/http_server.h"
#include "tensorflow_serving/model_servers/model_service_impl.h"
#include "tensorflow_serving/model_servers/prediction_service_impl.h"
#include "tensorflow_serving/model_servers/server_core.h"

namespace tensorflow {
namespace serving {
namespace main {

class Server {
 public:
  struct Options {
    //
    // gRPC Server options
    //
    tensorflow::int32 grpc_port = 8500;
    tensorflow::string grpc_channel_arguments;

    //
    // HTTP Server options.
    //
    tensorflow::int32 http_port = 0;
    tensorflow::int32 http_num_threads = 4.0 * port::NumSchedulableCPUs();
    tensorflow::int32 http_timeout_in_ms = 30000;  // 30 seconds.

    //
    // Model Server options.
    //
    bool enable_batching = false;
    float per_process_gpu_memory_fraction = 0;
    tensorflow::string batching_parameters_file;
    tensorflow::string model_name;
    tensorflow::int32 file_system_poll_wait_seconds = 1;
    bool flush_filesystem_caches = true;
    tensorflow::string model_base_path;
    tensorflow::string saved_model_tags;
    // Tensorflow session parallelism of zero means that both inter and intra op
    // thread pools will be auto configured.
    tensorflow::int64 tensorflow_session_parallelism = 0;
    tensorflow::string platform_config_file;
    tensorflow::string ssl_config_file;
    string model_config_file;
    bool enable_model_warmup = true;
    tensorflow::string monitoring_config_file;

    Options();
  };

  // Blocks the current thread waiting for servers (if any)
  // started as part of BuildAndStart() call.
  ~Server();

  // Build and start gRPC (and optionally HTTP) server, to be ready to
  // accept and process new requests over gRPC (and optionally HTTP/REST).
  Status BuildAndStart(const Options& server_options);

  // Wait for servers started in BuildAndStart() above to terminate.
  // This will block the current thread until termination is successful.
  void WaitForTermination();

 private:
  std::unique_ptr<ServerCore> server_core_;
  std::unique_ptr<ModelServiceImpl> model_service_;
  std::unique_ptr<PredictionServiceImpl> prediction_service_;
  std::unique_ptr<::grpc::Server> grpc_server_;
  std::unique_ptr<net_http::HTTPServerInterface> http_server_;
};

}  // namespace main
}  // namespace serving
}  // namespace tensorflow

#endif  // TENSORFLOW_SERVING_MODEL_SERVERS_SERVER_H_
