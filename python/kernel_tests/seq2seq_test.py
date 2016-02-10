# Copyright 2015 Google Inc. All Rights Reserved.
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

"""Tests for functional style sequence-to-sequence models."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import math
import random

# The pylint exception below is needed to make the code compatible with
# the open-source conversion script.
# pylint: disable=g-bad-import-order,unused-import
import tensorflow.python.platform

import numpy as np
import tensorflow as tf


class Seq2SeqTest(tf.test.TestCase):

  def testRNNDecoder(self):
    with self.test_session() as sess:
      with tf.variable_scope("root", initializer=tf.constant_initializer(0.5)):
        inp = [tf.constant(0.5, shape=[2, 2])] * 2
        _, enc_state = tf.nn.rnn(
            tf.nn.rnn_cell.GRUCell(2), inp, dtype=tf.float32)
        dec_inp = [tf.constant(0.4, shape=[2, 2])] * 3
        cell = tf.nn.rnn_cell.OutputProjectionWrapper(
            tf.nn.rnn_cell.GRUCell(2), 4)
        dec, mem = tf.nn.seq2seq.rnn_decoder(dec_inp, enc_state, cell)
        sess.run([tf.initialize_all_variables()])
        res = sess.run(dec)
        self.assertEqual(3, len(res))
        self.assertEqual((2, 4), res[0].shape)

        res = sess.run([mem])
        self.assertEqual((2, 2), res[0].shape)

  def testBasicRNNSeq2Seq(self):
    with self.test_session() as sess:
      with tf.variable_scope("root", initializer=tf.constant_initializer(0.5)):
        inp = [tf.constant(0.5, shape=[2, 2])] * 2
        dec_inp = [tf.constant(0.4, shape=[2, 2])] * 3
        cell = tf.nn.rnn_cell.OutputProjectionWrapper(
            tf.nn.rnn_cell.GRUCell(2), 4)
        dec, mem = tf.nn.seq2seq.basic_rnn_seq2seq(inp, dec_inp, cell)
        sess.run([tf.initialize_all_variables()])
        res = sess.run(dec)
        self.assertEqual(3, len(res))
        self.assertEqual((2, 4), res[0].shape)

        res = sess.run([mem])
        self.assertEqual((2, 2), res[0].shape)

  def testTiedRNNSeq2Seq(self):
    with self.test_session() as sess:
      with tf.variable_scope("root", initializer=tf.constant_initializer(0.5)):
        inp = [tf.constant(0.5, shape=[2, 2])] * 2
        dec_inp = [tf.constant(0.4, shape=[2, 2])] * 3
        cell = tf.nn.rnn_cell.OutputProjectionWrapper(
            tf.nn.rnn_cell.GRUCell(2), 4)
        dec, mem = tf.nn.seq2seq.tied_rnn_seq2seq(inp, dec_inp, cell)
        sess.run([tf.initialize_all_variables()])
        res = sess.run(dec)
        self.assertEqual(3, len(res))
        self.assertEqual((2, 4), res[0].shape)

        res = sess.run([mem])
        self.assertEqual(1, len(res))
        self.assertEqual((2, 2), res[0].shape)

  def testEmbeddingRNNDecoder(self):
    with self.test_session() as sess:
      with tf.variable_scope("root", initializer=tf.constant_initializer(0.5)):
        inp = [tf.constant(0.5, shape=[2, 2])] * 2
        cell = tf.nn.rnn_cell.BasicLSTMCell(2)
        _, enc_state = tf.nn.rnn(cell, inp, dtype=tf.float32)
        dec_inp = [tf.constant(i, tf.int32, shape=[2]) for i in range(3)]
        dec, mem = tf.nn.seq2seq.embedding_rnn_decoder(dec_inp, enc_state,
                                                       cell, 4)
        sess.run([tf.initialize_all_variables()])
        res = sess.run(dec)
        self.assertEqual(3, len(res))
        self.assertEqual((2, 2), res[0].shape)

        res = sess.run([mem])
        self.assertEqual(1, len(res))
        self.assertEqual((2, 4), res[0].shape)

  def testEmbeddingRNNSeq2Seq(self):
    with self.test_session() as sess:
      with tf.variable_scope("root", initializer=tf.constant_initializer(0.5)):
        enc_inp = [tf.constant(1, tf.int32, shape=[2]) for i in range(2)]
        dec_inp = [tf.constant(i, tf.int32, shape=[2]) for i in range(3)]
        cell = tf.nn.rnn_cell.BasicLSTMCell(2)
        dec, mem = tf.nn.seq2seq.embedding_rnn_seq2seq(
            enc_inp, dec_inp, cell, 2, 5)
        sess.run([tf.initialize_all_variables()])
        res = sess.run(dec)
        self.assertEqual(3, len(res))
        self.assertEqual((2, 5), res[0].shape)

        res = sess.run([mem])
        self.assertEqual((2, 4), res[0].shape)

        # Test externally provided output projection.
        w = tf.get_variable("proj_w", [2, 5])
        b = tf.get_variable("proj_b", [5])
        with tf.variable_scope("proj_seq2seq"):
          dec, _ = tf.nn.seq2seq.embedding_rnn_seq2seq(
              enc_inp, dec_inp, cell, 2, 5, output_projection=(w, b))
        sess.run([tf.initialize_all_variables()])
        res = sess.run(dec)
        self.assertEqual(3, len(res))
        self.assertEqual((2, 2), res[0].shape)

        # Test that previous-feeding model ignores inputs after the first.
        dec_inp2 = [tf.constant(0, tf.int32, shape=[2]) for _ in range(3)]
        tf.get_variable_scope().reuse_variables()
        d1, _ = tf.nn.seq2seq.embedding_rnn_seq2seq(
            enc_inp, dec_inp, cell, 2, 5,
            feed_previous=True)
        d2, _ = tf.nn.seq2seq.embedding_rnn_seq2seq(
            enc_inp, dec_inp2, cell, 2, 5,
            feed_previous=True)
        d3, _ = tf.nn.seq2seq.embedding_rnn_seq2seq(
            enc_inp, dec_inp2, cell, 2, 5,
            feed_previous=tf.constant(True))
        res1 = sess.run(d1)
        res2 = sess.run(d2)
        res3 = sess.run(d3)
        self.assertAllClose(res1, res2)
        self.assertAllClose(res1, res3)

  def testEmbeddingTiedRNNSeq2Seq(self):
    with self.test_session() as sess:
      with tf.variable_scope("root", initializer=tf.constant_initializer(0.5)):
        enc_inp = [tf.constant(1, tf.int32, shape=[2]) for i in range(2)]
        dec_inp = [tf.constant(i, tf.int32, shape=[2]) for i in range(3)]
        cell = tf.nn.rnn_cell.BasicLSTMCell(2)
        dec, mem = tf.nn.seq2seq.embedding_tied_rnn_seq2seq(
            enc_inp, dec_inp, cell, 5)
        sess.run([tf.initialize_all_variables()])
        res = sess.run(dec)
        self.assertEqual(3, len(res))
        self.assertEqual((2, 5), res[0].shape)

        res = sess.run([mem])
        self.assertEqual((2, 4), res[0].shape)

        # Test externally provided output projection.
        w = tf.get_variable("proj_w", [2, 5])
        b = tf.get_variable("proj_b", [5])
        with tf.variable_scope("proj_seq2seq"):
          dec, _ = tf.nn.seq2seq.embedding_tied_rnn_seq2seq(
              enc_inp, dec_inp, cell, 5, output_projection=(w, b))
        sess.run([tf.initialize_all_variables()])
        res = sess.run(dec)
        self.assertEqual(3, len(res))
        self.assertEqual((2, 2), res[0].shape)

        # Test that previous-feeding model ignores inputs after the first.
        dec_inp2 = [tf.constant(0, tf.int32, shape=[2])] * 3
        tf.get_variable_scope().reuse_variables()
        d1, _ = tf.nn.seq2seq.embedding_tied_rnn_seq2seq(
            enc_inp, dec_inp, cell, 5,
            feed_previous=True)
        d2, _ = tf.nn.seq2seq.embedding_tied_rnn_seq2seq(
            enc_inp, dec_inp2, cell, 5,
            feed_previous=True)
        d3, _ = tf.nn.seq2seq.embedding_tied_rnn_seq2seq(
            enc_inp, dec_inp2, cell, 5, feed_previous=tf.constant(True))
        res1 = sess.run(d1)
        res2 = sess.run(d2)
        res3 = sess.run(d3)
        self.assertAllClose(res1, res2)
        self.assertAllClose(res1, res3)

  def testAttentionDecoder1(self):
    with self.test_session() as sess:
      with tf.variable_scope("root", initializer=tf.constant_initializer(0.5)):
        cell = tf.nn.rnn_cell.GRUCell(2)
        inp = [tf.constant(0.5, shape=[2, 2])] * 2
        enc_outputs, enc_state = tf.nn.rnn(cell, inp, dtype=tf.float32)
        attn_states = tf.concat(1, [tf.reshape(e, [-1, 1, cell.output_size])
                                    for e in enc_outputs])
        dec_inp = [tf.constant(0.4, shape=[2, 2])] * 3
        dec, mem = tf.nn.seq2seq.attention_decoder(
            dec_inp, enc_state,
            attn_states, cell, output_size=4)
        sess.run([tf.initialize_all_variables()])
        res = sess.run(dec)
        self.assertEqual(3, len(res))
        self.assertEqual((2, 4), res[0].shape)

        res = sess.run([mem])
        self.assertEqual((2, 2), res[0].shape)

  def testAttentionDecoder2(self):
    with self.test_session() as sess:
      with tf.variable_scope("root", initializer=tf.constant_initializer(0.5)):
        cell = tf.nn.rnn_cell.GRUCell(2)
        inp = [tf.constant(0.5, shape=[2, 2])] * 2
        enc_outputs, enc_state = tf.nn.rnn(cell, inp, dtype=tf.float32)
        attn_states = tf.concat(1, [tf.reshape(e, [-1, 1, cell.output_size])
                                    for e in enc_outputs])
        dec_inp = [tf.constant(0.4, shape=[2, 2])] * 3
        dec, mem = tf.nn.seq2seq.attention_decoder(
            dec_inp, enc_state,
            attn_states, cell, output_size=4,
            num_heads=2)
        sess.run([tf.initialize_all_variables()])
        res = sess.run(dec)
        self.assertEqual(3, len(res))
        self.assertEqual((2, 4), res[0].shape)

        res = sess.run([mem])
        self.assertEqual((2, 2), res[0].shape)

  def testEmbeddingAttentionDecoder(self):
    with self.test_session() as sess:
      with tf.variable_scope("root", initializer=tf.constant_initializer(0.5)):
        inp = [tf.constant(0.5, shape=[2, 2])] * 2
        cell = tf.nn.rnn_cell.GRUCell(2)
        enc_outputs, enc_state = tf.nn.rnn(cell, inp, dtype=tf.float32)
        attn_states = tf.concat(1, [tf.reshape(e, [-1, 1, cell.output_size])
                                    for e in enc_outputs])
        dec_inp = [tf.constant(i, tf.int32, shape=[2]) for i in range(3)]
        dec, mem = tf.nn.seq2seq.embedding_attention_decoder(
            dec_inp, enc_state,
            attn_states, cell, 4,
            output_size=3)
        sess.run([tf.initialize_all_variables()])
        res = sess.run(dec)
        self.assertEqual(3, len(res))
        self.assertEqual((2, 3), res[0].shape)

        res = sess.run([mem])
        self.assertEqual((2, 2), res[0].shape)

  def testEmbeddingAttentionSeq2Seq(self):
    with self.test_session() as sess:
      with tf.variable_scope("root", initializer=tf.constant_initializer(0.5)):
        enc_inp = [tf.constant(1, tf.int32, shape=[2]) for i in range(2)]
        dec_inp = [tf.constant(i, tf.int32, shape=[2]) for i in range(3)]
        cell = tf.nn.rnn_cell.BasicLSTMCell(2)
        dec, mem = tf.nn.seq2seq.embedding_attention_seq2seq(
            enc_inp, dec_inp, cell, 2, 5)
        sess.run([tf.initialize_all_variables()])
        res = sess.run(dec)
        self.assertEqual(3, len(res))
        self.assertEqual((2, 5), res[0].shape)

        res = sess.run([mem])
        self.assertEqual((2, 4), res[0].shape)

        # Test externally provided output projection.
        w = tf.get_variable("proj_w", [2, 5])
        b = tf.get_variable("proj_b", [5])
        with tf.variable_scope("proj_seq2seq"):
          dec, _ = tf.nn.seq2seq.embedding_attention_seq2seq(
              enc_inp, dec_inp, cell, 2, 5, output_projection=(w, b))
        sess.run([tf.initialize_all_variables()])
        res = sess.run(dec)
        self.assertEqual(3, len(res))
        self.assertEqual((2, 2), res[0].shape)

        # Test that previous-feeding model ignores inputs after the first.
        dec_inp2 = [tf.constant(0, tf.int32, shape=[2]) for _ in range(3)]
        tf.get_variable_scope().reuse_variables()
        d1, _ = tf.nn.seq2seq.embedding_attention_seq2seq(
            enc_inp, dec_inp, cell, 2, 5, feed_previous=True)
        d2, _ = tf.nn.seq2seq.embedding_attention_seq2seq(
            enc_inp, dec_inp2, cell, 2, 5, feed_previous=True)
        d3, _ = tf.nn.seq2seq.embedding_attention_seq2seq(
            enc_inp, dec_inp2, cell, 2, 5, feed_previous=tf.constant(True))
        res1 = sess.run(d1)
        res2 = sess.run(d2)
        res3 = sess.run(d3)
        self.assertAllClose(res1, res2)
        self.assertAllClose(res1, res3)

  def testOne2ManyRNNSeq2Seq(self):
    with self.test_session() as sess:
      with tf.variable_scope("root", initializer=tf.constant_initializer(0.5)):
        enc_inp = [tf.constant(1, tf.int32, shape=[2]) for i in range(2)]
        dec_inp_dict = {}
        dec_inp_dict["0"] = [
            tf.constant(i, tf.int32, shape=[2]) for i in range(3)]
        dec_inp_dict["1"] = [
            tf.constant(i, tf.int32, shape=[2]) for i in range(4)]
        dec_symbols_dict = {"0": 5, "1": 6}
        cell = tf.nn.rnn_cell.BasicLSTMCell(2)
        outputs_dict, state_dict = tf.nn.seq2seq.one2many_rnn_seq2seq(
            enc_inp, dec_inp_dict, cell, 2, dec_symbols_dict)

        sess.run([tf.initialize_all_variables()])
        res = sess.run(outputs_dict["0"])
        self.assertEqual(3, len(res))
        self.assertEqual((2, 5), res[0].shape)
        res = sess.run(outputs_dict["1"])
        self.assertEqual(4, len(res))
        self.assertEqual((2, 6), res[0].shape)
        res = sess.run([state_dict["0"]])
        self.assertEqual((2, 4), res[0].shape)
        res = sess.run([state_dict["1"]])
        self.assertEqual((2, 4), res[0].shape)

        # Test that previous-feeding model ignores inputs after the first, i.e.
        # dec_inp_dict2 has different inputs from dec_inp_dict after the first
        # time-step.
        dec_inp_dict2 = {}
        dec_inp_dict2["0"] = [
            tf.constant(0, tf.int32, shape=[2]) for _ in range(3)]
        dec_inp_dict2["1"] = [
            tf.constant(0, tf.int32, shape=[2]) for _ in range(4)]
        tf.get_variable_scope().reuse_variables()
        outputs_dict1, _ = tf.nn.seq2seq.one2many_rnn_seq2seq(
            enc_inp, dec_inp_dict, cell, 2, dec_symbols_dict,
            feed_previous=True)
        outputs_dict2, _ = tf.nn.seq2seq.one2many_rnn_seq2seq(
            enc_inp, dec_inp_dict2, cell, 2, dec_symbols_dict,
            feed_previous=True)
        outputs_dict3, _ = tf.nn.seq2seq.one2many_rnn_seq2seq(
            enc_inp, dec_inp_dict2, cell, 2, dec_symbols_dict,
            feed_previous=tf.constant(True))
        res1 = sess.run(outputs_dict1["0"])
        res2 = sess.run(outputs_dict2["0"])
        res3 = sess.run(outputs_dict3["0"])
        self.assertAllClose(res1, res2)
        self.assertAllClose(res1, res3)

  def testSequenceLoss(self):
    with self.test_session() as sess:
      logits = [tf.constant(i + 0.5, shape=[2, 5]) for i in range(3)]
      targets = [tf.constant(i, tf.int32, shape=[2]) for i in range(3)]
      weights = [tf.constant(1.0, shape=[2]) for i in range(3)]

      average_loss_per_example = tf.nn.seq2seq.sequence_loss(
          logits, targets, weights,
          average_across_timesteps=True,
          average_across_batch=True)
      res = sess.run(average_loss_per_example)
      self.assertAllClose(1.60944, res)

      average_loss_per_sequence = tf.nn.seq2seq.sequence_loss(
          logits, targets, weights,
          average_across_timesteps=False,
          average_across_batch=True)
      res = sess.run(average_loss_per_sequence)
      self.assertAllClose(4.828314, res)

      total_loss = tf.nn.seq2seq.sequence_loss(
          logits, targets, weights,
          average_across_timesteps=False,
          average_across_batch=False)
      res = sess.run(total_loss)
      self.assertAllClose(9.656628, res)

  def testSequenceLossByExample(self):
    with self.test_session() as sess:
      output_classes = 5
      logits = [tf.constant(i + 0.5, shape=[2, output_classes])
                for i in range(3)]
      targets = [tf.constant(i, tf.int32, shape=[2]) for i in range(3)]
      weights = [tf.constant(1.0, shape=[2]) for i in range(3)]

      average_loss_per_example = tf.nn.seq2seq.sequence_loss_by_example(
          logits, targets, weights,
          average_across_timesteps=True)
      res = sess.run(average_loss_per_example)
      self.assertAllClose(np.asarray([1.609438, 1.609438]), res)

      loss_per_sequence = tf.nn.seq2seq.sequence_loss_by_example(
          logits, targets, weights,
          average_across_timesteps=False)
      res = sess.run(loss_per_sequence)
      self.assertAllClose(np.asarray([4.828314, 4.828314]), res)

  def testModelWithBucketsScope(self):
    """Test that variable scope reuse is not reset after model_with_buckets."""
    classes = 10
    buckets = [(4, 4), (8, 8)]

    with self.test_session():
      # Here comes a sample Seq2Seq model using GRU cells.
      def SampleGRUSeq2Seq(enc_inp, dec_inp, weights):
        """Example sequence-to-sequence model that uses GRU cells."""
        def GRUSeq2Seq(enc_inp, dec_inp):
          cell = tf.nn.rnn_cell.MultiRNNCell([tf.nn.rnn_cell.GRUCell(24)] * 2)
          return tf.nn.seq2seq.embedding_attention_seq2seq(
              enc_inp, dec_inp, cell, classes, classes)
        targets = [dec_inp[i+1] for i in range(len(dec_inp) - 1)] + [0]
        return tf.nn.seq2seq.model_with_buckets(
            enc_inp, dec_inp, targets, weights, buckets, GRUSeq2Seq)

      # Now we construct the copy model.
      inp = [tf.placeholder(tf.int32, shape=[None]) for _ in range(8)]
      out = [tf.placeholder(tf.int32, shape=[None]) for _ in range(8)]
      weights = [tf.ones_like(inp[0], dtype=tf.float32) for _ in range(8)]
      with tf.variable_scope("root"):
        _ = SampleGRUSeq2Seq(inp, out, weights)
        # Now check that we did not accidentally set reuse.
        self.assertEqual(False, tf.get_variable_scope().reuse)

  def testModelWithBuckets(self):
    """Larger tests that does full sequence-to-sequence model training."""
    # We learn to copy 10 symbols in 2 buckets: length 4 and length 8.
    classes = 10
    buckets = [(4, 4), (8, 8)]
    perplexities = [[], []]  # Results for each bucket.
    tf.set_random_seed(111)
    random.seed(111)
    np.random.seed(111)

    with self.test_session() as sess:
      # We use sampled softmax so we keep output projection separate.
      w = tf.get_variable("proj_w", [24, classes])
      w_t = tf.transpose(w)
      b = tf.get_variable("proj_b", [classes])
      # Here comes a sample Seq2Seq model using GRU cells.
      def SampleGRUSeq2Seq(enc_inp, dec_inp, weights):
        """Example sequence-to-sequence model that uses GRU cells."""
        def GRUSeq2Seq(enc_inp, dec_inp):
          cell = tf.nn.rnn_cell.MultiRNNCell([tf.nn.rnn_cell.GRUCell(24)] * 2)
          return tf.nn.seq2seq.embedding_attention_seq2seq(
              enc_inp, dec_inp, cell, classes, classes,
              output_projection=(w, b))
        targets = [dec_inp[i+1] for i in range(len(dec_inp) - 1)] + [0]
        def SampledLoss(inputs, labels):
          labels = tf.reshape(labels, [-1, 1])
          return tf.nn.sampled_softmax_loss(w_t, b, inputs, labels, 8, classes)
        return tf.nn.seq2seq.model_with_buckets(
            enc_inp, dec_inp, targets, weights, buckets, GRUSeq2Seq,
            softmax_loss_function=SampledLoss)

      # Now we construct the copy model.
      batch_size = 8
      inp = [tf.placeholder(tf.int32, shape=[None]) for _ in range(8)]
      out = [tf.placeholder(tf.int32, shape=[None]) for _ in range(8)]
      weights = [tf.ones_like(inp[0], dtype=tf.float32) for _ in range(8)]
      with tf.variable_scope("root"):
        _, losses = SampleGRUSeq2Seq(inp, out, weights)
        updates = []
        params = tf.all_variables()
        optimizer = tf.train.AdamOptimizer(0.03, epsilon=1e-5)
        for i in range(len(buckets)):
          full_grads = tf.gradients(losses[i], params)
          grads, _ = tf.clip_by_global_norm(full_grads, 30.0)
          update = optimizer.apply_gradients(zip(grads, params))
          updates.append(update)
        sess.run([tf.initialize_all_variables()])
      steps = 6
      for _ in range(steps):
        bucket = random.choice(np.arange(len(buckets)))
        length = buckets[bucket][0]
        i = [np.array([np.random.randint(9) + 1 for _ in range(batch_size)],
                      dtype=np.int32) for _ in range(length)]
        # 0 is our "GO" symbol here.
        o = [np.array([0] * batch_size, dtype=np.int32)] + i
        feed = {}
        for i1, i2, o1, o2 in zip(inp[:length], i[:length],
                                  out[:length], o[:length]):
          feed[i1.name] = i2
          feed[o1.name] = o2
        if length < 8:  # For the 4-bucket, we need the 5th as target.
          feed[out[length].name] = o[length]
        res = sess.run([updates[bucket], losses[bucket]], feed)
        perplexities[bucket].append(math.exp(float(res[1])))
      for bucket in range(len(buckets)):
        if len(perplexities[bucket]) > 1:  # Assert that perplexity went down.
          self.assertLess(perplexities[bucket][-1], perplexities[bucket][0])


if __name__ == "__main__":
  tf.test.main()
