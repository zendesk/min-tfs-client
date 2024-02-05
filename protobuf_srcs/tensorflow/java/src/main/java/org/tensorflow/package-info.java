/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

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

/**
 * Defines classes to build, save, load and execute TensorFlow models.
 *
  *<aside class="warning">
 * <b>Warning:</b> This API is deprecated and will be removed in a future
 *       version of TensorFlow after <a href="https://tensorflow.org/java">the replacement</a>
 *       is stable.
 *</aside>
 *
 * <p>To get started, see the <a href="https://tensorflow.org/install/lang_java_legacy">
 * installation instructions.</a></p>
 *
 * <p>The <a
 * href="https://www.tensorflow.org/code/tensorflow/java/src/main/java/org/tensorflow/examples/LabelImage.java">LabelImage</a>
 * example demonstrates use of this API to classify images using a pre-trained <a
 * href="http://arxiv.org/abs/1512.00567">Inception</a> architecture convolutional neural network.
 * It demonstrates:
 *
 * <ul>
 *   <li>Graph construction: using the OperationBuilder class to construct a graph to decode, resize
 *       and normalize a JPEG image.
 *   <li>Model loading: Using Graph.importGraphDef() to load a pre-trained Inception model.
 *   <li>Graph execution: Using a Session to execute the graphs and find the best label for an
 *       image.
 * </ul>
 *
 * <p>Additional examples can be found in the <a
 * href="https://github.com/tensorflow/java">tensorflow/java</a> GitHub repository.
 */
package org.tensorflow;
