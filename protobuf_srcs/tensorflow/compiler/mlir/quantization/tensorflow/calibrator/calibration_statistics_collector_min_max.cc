/* Copyright 2023 The TensorFlow Authors. All Rights Reserved.

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
#include "tensorflow/compiler/mlir/quantization/tensorflow/calibrator/calibration_statistics_collector_min_max.h"

#include <algorithm>
#include <limits>
#include <optional>

#include "tensorflow/compiler/mlir/quantization/tensorflow/calibrator/calibration_statistics.pb.h"

namespace tensorflow {
namespace calibrator {

void CalibrationStatisticsCollectorMinMax::ClearData() {
  // global_min will be updated by std::min(global_min, input_value) so
  // it is initialized with the value numeric_limits<float>::max().
  min_max_statistics_.set_global_min(std::numeric_limits<float>::max());

  // global_max will be updated by std::max(global_max, input_value) so it
  // is initialized with the value numeric_limits<float>::lowest().
  min_max_statistics_.set_global_max(std::numeric_limits<float>::lowest());
}

void CalibrationStatisticsCollectorMinMax::Collect(const float *data,
                                                   const unsigned int N) {
  float input_min = min_max_statistics_.global_min();
  float input_max = min_max_statistics_.global_max();

  for (int i = 0; i < N; ++i) {
    input_min = std::min(input_min, data[i]);
    input_max = std::max(input_max, data[i]);
  }

  min_max_statistics_.set_global_min(input_min);
  min_max_statistics_.set_global_max(input_max);
}

std::optional<CalibrationStatistics>
CalibrationStatisticsCollectorMinMax::GetStatistics() const {
  if (min_max_statistics_.global_min() == std::numeric_limits<float>::max())
    return std::nullopt;

  CalibrationStatistics statistics;
  statistics.mutable_min_max_statistics()->CopyFrom(min_max_statistics_);

  return statistics;
}

}  // namespace calibrator
}  // namespace tensorflow
