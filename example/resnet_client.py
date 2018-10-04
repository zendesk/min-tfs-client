# Copyright 2018 Google Inc. All Rights Reserved.
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
"""A client that performs inferences on a ResNet model using the REST API.

The client downloads a test image of a cat, queries the server over the REST API
with the test image repeatedly and measures how long it takes to respond.

Typical usage example:

    resnet_client.py
"""

from __future__ import print_function

import base64
import requests

SERVER_URL = 'http://localhost:8501/v1/models/resnet:predict'
IMAGE_URL = 'https://tensorflow.org/images/blogs/serving/cat.jpg'


def main():
  # Download the image
  dl_request = requests.get(IMAGE_URL, stream=True)
  dl_request.raise_for_status()

  # Compose a JSON Predict request (send JPEG image in base64).
  predict_request = '{"instances" : [{"b64": "%s"}]}' % base64.b64encode(
      dl_request.content)

  # Send few requests to warm-up the model.
  for _ in xrange(3):
    response = requests.post(SERVER_URL, data=predict_request)
    response.raise_for_status()

  # Send few actual requests and report average latency.
  total_time = 0
  num_requests = 10
  for _ in xrange(num_requests):
    response = requests.post(SERVER_URL, data=predict_request)
    response.raise_for_status()
    total_time += response.elapsed.total_seconds()
    prediction = response.json()['predictions'][0]

  print('Prediction class: {}, avg latency: {} ms'.format(
      prediction['classes'], (total_time*1000)/num_requests))


if __name__ == '__main__':
  main()
