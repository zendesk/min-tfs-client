#!/usr/bin/env bash
# Copyright 2019 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
#
# Tests the microcontroller code for bluepill platform

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR=${SCRIPT_DIR}/../../../../../..
cd ${ROOT_DIR}
pwd

source tensorflow/lite/experimental/micro/tools/ci_build/helper_functions.sh

readable_run make -f tensorflow/lite/experimental/micro/tools/make/Makefile clean

# TODO(b/143715361): downloading first to allow for parallel builds.
readable_run make -f tensorflow/lite/experimental/micro/tools/make/Makefile TARGET=sparkfun_edge third_party_downloads

# TODO(b/143286954): Run all the tests once they pass.
readable_run make -j8 -f tensorflow/lite/experimental/micro/tools/make/Makefile TARGET=bluepill kernel_add_test

