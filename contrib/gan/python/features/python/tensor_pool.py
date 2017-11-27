# Copyright 2017 The TensorFlow Authors. All Rights Reserved.
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
"""A tensor pool stores values from an input tensor and returns a stored one.

See the following papers for more details.
1) `Learning from simulated and unsupervised images through adversarial
    training` (https://arxiv.org/abs/1612.07828).
2) `Unpaired Image-to-Image Translation using Cycle-Consistent Adversarial
    Networks` (https://arxiv.org/abs/1703.10593).
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.contrib.gan.python.features.python import tensor_pool_impl
# pylint: disable=wildcard-import
from tensorflow.contrib.gan.python.features.python.tensor_pool_impl import *
# pylint: enable=wildcard-import
from tensorflow.python.util.all_util import remove_undocumented

__all__ = tensor_pool_impl.__all__
remove_undocumented(__name__, __all__)
