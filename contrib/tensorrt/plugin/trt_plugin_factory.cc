/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/contrib/tensorrt/plugin/trt_plugin_factory.h"

#if GOOGLE_CUDA
#if GOOGLE_TENSORRT

namespace tensorflow {
namespace tensorrt {

PluginTensorRT* PluginFactoryTensorRT::createPlugin(const char* layerName,
                                                    const void* serial_data,
                                                    size_t serial_length) {
  size_t parsed_byte = 0;
  // extract op_name from serial_data
  size_t encoded_op_name =
      ExtractOpName(serial_data, serial_length, parsed_byte);

  if (!IsPlugin(encoded_op_name)) {
    return nullptr;
  }

  // should I lock plugins here?
  instance_m_.lock();
  auto plugin_ptr =
      plugin_registry_[encoded_op_name].first(serial_data, serial_length);
  // string op_name = "IncPluginTRT";
  // auto plugin_ptr = plugin_registry_[EncodeLayerName(&op_name)].second();
  // auto plugin_ptr = plugin_registry_.begin()->second.second();
  owned_plugins_.emplace_back(plugin_ptr);
  instance_m_.unlock();

  return plugin_ptr;
}

PluginTensorRT* PluginFactoryTensorRT::CreatePlugin(const string* op_name) {
  if (!IsPlugin(op_name)) return nullptr;

  instance_m_.lock();
  auto plugin_ptr = plugin_registry_[EncodeLayerName(op_name)].second();
  owned_plugins_.emplace_back(plugin_ptr);
  instance_m_.unlock();

  return plugin_ptr;
}

bool PluginFactoryTensorRT::RegisterPlugin(
    const string* op_name, PluginDeserializeFunc deserialize_func,
    PluginConstructFunc construct_func) {
  if (IsPlugin(op_name)) return false;

  // get instance_m_ first before write to registry;
  instance_m_.lock();
  auto ret = plugin_registry_.emplace(
      EncodeLayerName(op_name),
      std::make_pair(deserialize_func, construct_func));
  instance_m_.unlock();

  return ret.second;
}

void PluginFactoryTensorRT::DestroyPlugins() { return; }

}  // namespace tensorrt
}  // namespace tensorflow

#endif  // GOOGLE_CUDA
#endif  // GOOGLE_TENSORRT
