# Copyright 2016 Google Inc. All Rights Reserved.
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
"""Training helper that checkpoints models and computes summaries."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import threading
import time

from tensorflow.core.framework.summary_pb2 import Summary
from tensorflow.core.util.event_pb2 import SessionLog
from tensorflow.python.framework import dtypes
from tensorflow.python.framework import ops
from tensorflow.python.ops import control_flow_ops
from tensorflow.python.ops import data_flow_ops
from tensorflow.python.ops import logging_ops
from tensorflow.python.ops import variables
from tensorflow.python.platform import logging
from tensorflow.python.training import coordinator
from tensorflow.python.training import saver as saver_mod
from tensorflow.python.training import session_manager as session_manager_mod
from tensorflow.python.training import summary_io
from tensorflow.python.training import training_util


class Supervisor(object):
  """A training helper that checkpoints models and computes summaries.

  The Supervisor is a small wrapper around a `Coordinator`, a `Saver`,
  and a `SessionManager` that takes care of common needs of Tensorflow
  training programs.

  #### Use for a single program

  ```python
  with tf.Graph().as_default():
    ...add operations to the graph...
    # Create a Supervisor that will checkpoint the model in '/tmp/mydir'.
    sv = Supervisor(logdir='/tmp/mydir')
    # Get a Tensorflow session.
    sess = sv.prepare_or_wait_for_session(FLAGS.master)
    # Use the session to train the graph.
    while not sv.should_stop():
      sess.run(<my_train_op>)
    # Ask for all the services to stop.
    sv.stop()
   ```

  After the call to `prepare_or_wait_for_session()`, all `Variables` in
  the `Graph` have been initialized.  In addition, a few services have
  been started to checkpoint the model and fetch summaries.

  If the program crashes and you restart it, the call to
  `prepare_or_wait_for_session()` automatically reinitializes the Variables
  from most recent checkpoint.

  If any of the services raises an exception, it will ask the
  Supervisor to stop.  In that case `should_stop()` will return True
  and you should stop your training loop.

  Finish by calling `stop()` to cleanly wait for the services to complete.
  If a service thread raised an exception, it is re-raised in the `stop()`
  call so your program can easily report it.

  #### Use for multiple replicas

  To train with replicas you deploy the same program in a `Cluster`.
  One of the tasks must be identified as the *chief*: the task that handles
  initialization, checkpoints, summaries, and recovery.  The other tasks
  depend on the *chief* for these services.

  The only change you have to do to the single program code is to indicate
  if the program is running as the *chief*.

  ```python
  # Choose a task as the chief. This could be based on server_def.task_index,
  # or job_def.name, or job_def.tasks. It's entirely up to the end user.
  # But there can be only one *chief*.
  is_chief = (server_def.task_index == 0)
  server = tf.train.Server(server_def)

  with tf.Graph().as_default():
    ...add operations to the graph...
    # Create a Supervisor that uses log directory on a shared file system.
    # Indicate if you are the 'chief'
    sv = Supervisor(logdir='/shared_directory/...', is_chief=is_chief)
    # Get a Session in a TensorFlow server on the cluster.
    sess = sv.prepare_or_wait_for_session(server.target)
    # Use the session to train the graph.
    while not sv.should_stop():
      sess.run(<my_train_op>)
    # Ask for all the services to stop.
    sv.stop()
  ```

  In the *chief* task, the `Supervisor` works exactly as in the first
  example above.  In the other tasks `prepare_or_wait_for_session()`
  waits for the Model to have been intialized before returning a
  session to the training code.

  If one of the tasks crashes and restarts,
  `prepare_or_wait_for_session()` checks if the Model is initialized.
  If yes, it just creates a session and returns it to the training
  code that proceeds normally.  If the model needs to be initialized,
  the chief task takes care of reinitializing it; the other tasks just
  wait for the model to have been initialized.

  NOTE: This modified program still works fine as a single program.
  The single program marks itself as the chief.

  #### What `master` string to use

  Whether you are running on your machine or in the cluster you can use the
  following values for the --master flag:

  * Specifying `''` requests an in-process session that does not use RPC.

  * Specifying `'local'` requests a session that uses the RPC-based
    "Master interface" to run TensorFlow programs. See
    [`tf.train.Server.create_local_server()`](#Server.create_local_server) for
    details.

  * Specifying `'grpc://hostname:port'` requests a session that uses
    the RPC interface to a specific , and also allows the in-process
    master to access remote tensorflow workers. Often, it is
    appropriate to pass `server.target` (for some `tf.train.Server`
    named `server).

  #### Advanced use

  ##### Launching additional services

  `prepare_or_wait_for_session()` launches the Checkpoint and Summary
  services (threads).  If you need more services to run you can simply
  launch them after `prepare_or_wait_for_session()` returns.  The Supervisor
  uses a Coordinator to help multiple threads stop together, so pass that
  coordinator (`sv.coord`) to the threads you launch.

  Example: Start a QueueRunner to prefetch inputs.

    ```python
    ...build the model with a QueueRunner to prefetch inputs...
    qr = QueueRunner(input_queue, [enqueue_op])
    ...
    sv = Supervisor(logdir='/tmp/mydir')
    sess = sv.prepare_or_wait_for_session(FLAGS.master)
    # Start the queue runner threads.
    threads = qr.create_threads(sess, sv.coord, start=True)
    # Catch OutOfRangeError, which signals that your input queue is exhausted.
    try:
      while not sv.should_stop():
        sess.run(my_train_op)
    except tf.errors.OutOfRangeError:
      pass
    # Wait for the QueueRunner and service threads to complete.
    sv.stop(threads)
    ```

  Note: Starting `QueueRunner` threads is very common, to the Supervisor
  provides a convenience method named `start_queue_runners()`.  If you use
  that method you do not have to keep track of the started threads and
  can just call `stop()` normally:

    ```python
    ...build the model with a QueueRunner to prefetch inputs...
    qr = QueueRunner(input_queue, [enqueue_op])
    ...
    sv = Supervisor(logdir='/tmp/mydir')
    sess = sv.prepare_or_wait_for_session(FLAGS.master)
    # Start the queue runner threads.
    sv.start_queue_runners(sess, [qr])
    # Catch OutOfRangeError, which signals that your input queue is exhausted.
    try:
      while not sv.should_stop():
        sess.run(my_train_op)
    except tf.errors.OutOfRangeError:
      pass
    # Wait for the QueueRunner and service threads to complete.
    sv.stop()
    ```

  ##### Launching fewer services

  `prepare_or_wait_for_session()` launches the "summary" and "checkpoint"
  services (threads) which use either the optionally `summary_op`
  and `saver` passed to the constructor, or default ones created
  automatically by the `Supervisor`.  If you want to run your own summary
  and checkpointing logic, disable these services by passing `None` to the
  `summary_op` and `saver` parameters.

  Example: Create summaries manually every 100 steps in the chief.

    ```python
    # Create a Supervisor with no automatic summaries.
    sv = Supervisor(logdir='/tmp/mydir', is_chief=is_chief, summary_op=None)
    # As summary_op was None, prepare_or_wait_for_session() does not start the
    # summary thread.
    sess = sv.prepare_or_wait_for_session(FLAGS.master)
    for step in xrange(1000000):
      if is_chief and step % 100 == 0:
        # Create the summary every 100 chief steps.
        sv.summary_computed(sess, sess.run(my_summary_op))
      else:
        # Train normally
        sess.run(my_train_op)
    ```

  ##### Custom model initialization

  `prepare_or_wait_for_session()` only supports initializing the model
  by running an `init_op`.  If you have special initialization needs,
  use `local_init_op`.

  @@__init__
  @@prepare_or_wait_for_session
  @@start_standard_services
  @@summary_computed

  @@stop
  @@request_stop
  @@should_stop
  @@stop_on_exception
  @@wait_for_stop
  """

  # Value to pass for the 'ready_op', 'init_op', 'summary_op', 'saver',
  # and 'global_step' parameters of Supervisor.__init__() to indicate that
  # the default behavior should be used.
  USE_DEFAULT = 0

  # Protects _TENSORFLOW_LAUNCHED
  _launch_lock = threading.Lock()

  # True if we have already launched the tensorflow in-process server.
  _TENSORFLOW_LAUNCHED = False

  def __init__(self, graph=None, ready_op=USE_DEFAULT, is_chief=True,
               init_op=USE_DEFAULT, init_feed_dict=None,
               local_init_op=USE_DEFAULT, logdir=None,
               summary_op=USE_DEFAULT, saver=USE_DEFAULT,
               global_step=USE_DEFAULT, save_summaries_secs=120,
               save_model_secs=600, recovery_wait_secs=30,
               checkpoint_basename="model.ckpt", session_manager=None):
    """Create a `Supervisor`.

    Args:
      graph: A `Graph`.  The graph that the model will use.  Defaults to the
        default `Graph`.  The supervisor may add operations to the graph before
        creating a session, but the graph should not be modified by the caller
        after passing it to the supervisor.
      ready_op: `Operation` to check if the model is initialized.  This
        operation is run by supervisors in `prepare_or_wait_for_session()` to
        check if the model is ready to use. The model is considered ready if
        that operation succeeds.  Defaults to the operation returned from
        `tf.assert_variables_initialized()`  If `None`, the model is not checked
        for readiness.
      is_chief: If True, create a chief supervisor in charge of initializing
        and restoring the model.  If False, create a supervisor that relies
        on a chief supervisor for inits and restore.
      init_op: `Operation`.  Used by chief supervisors to initialize the model
        when it can not be recovered.  Defaults to an `Operation` that
        initializes all variables.  If `None`, no initialization is done
        automatically.
      init_feed_dict: A dictionary that maps `Tensor` objects to feed values.
        This feed dictionary will be used when `init_op` is evaluated.
      local_init_op: `Operation`. Used by all supervisors to run initializations
        that should run for every new supervisor instance. By default these
        are table initializers and initializers for local variables.
        If `None`, no further per supervisor-instance initialization is
        done automatically.
      logdir: A string.  Optional path to a directory where to checkpoint the
        model and log events for the visualizer.  Used by chief supervisors.
        The directory will be created if it does not exist.
      summary_op: An `Operation` that returns a Summary for the event logs.
        Used by chief supervisors if a `logdir` was specified.  Defaults to the
        operation returned from merge_all_summaries().  If `None`, summaries are
        not computed automatically.
      saver: A Saver object.  Used by chief supervisors if a `logdir` was
        specified.  Defaults to the saved returned by Saver().
        If `None`, the model is not saved automatically.
      global_step: An integer Tensor of size 1 that counts steps.  The value
        from 'global_step' is used in summaries and checkpoint filenames.
        Default to the op named 'global_step' in the graph if it exists, is of
        rank 1, size 1, and of type tf.int32 ot tf.int64.  If `None` the global
        step is not recorded in summaries and checkpoint files.  Used by chief
        supervisors if a `logdir` was specified.
      save_summaries_secs: Number of seconds between the computation of
        summaries for the event log.  Defaults to 120 seconds.  Pass 0 to
        disable summaries.
      save_model_secs: Number of seconds between the creation of model
        checkpoints.  Defaults to 600 seconds.  Pass 0 to disable checkpoints.
      recovery_wait_secs: Number of seconds between checks that the model
        is ready.  Used by supervisors when waiting for a chief supervisor
        to initialize or restore the model.  Defaults to 30 seconds.
      checkpoint_basename: The basename for checkpoint saving.
      session_manager: `SessionManager`, which manages Session creation and
        recovery. If it is `None`, a default `SessionManager` will be created
        with the set of arguments passed in for backwards compatibility.

    Returns:
      A `Supervisor`.
    """
    # Set default values of arguments.
    if graph is None:
      graph = ops.get_default_graph()
    with graph.as_default():
      self._init_ready_op(ready_op=ready_op)
      self._init_init_op(init_op=init_op, init_feed_dict=init_feed_dict)
      self._init_local_init_op(local_init_op=local_init_op)
      self._init_saver(saver=saver)
      self._init_summary_op(summary_op=summary_op)
      self._init_global_step(global_step=global_step)
    self._graph = graph
    self._is_chief = is_chief
    self._logdir = logdir
    self._save_summaries_secs = save_summaries_secs
    self._save_model_secs = save_model_secs
    self._recovery_wait_secs = recovery_wait_secs
    self._coord = coordinator.Coordinator()
    if logdir:
      self._save_path = os.path.join(self._logdir, checkpoint_basename)
      self._summary_writer = summary_io.SummaryWriter(self._logdir)
    else:
      self._save_path = None
      self._summary_writer = None
    self._init_session_manager(session_manager=session_manager)
    self._started_threads = []
    self._verify_setup()
    # The graph is not allowed to change anymore.
    graph.finalize()

  def _init_session_manager(self, session_manager=None):
    if session_manager is None:
      self._session_manager = session_manager_mod.SessionManager(
          local_init_op=self._local_init_op,
          ready_op=self._ready_op, graph=self._graph,
          recovery_wait_secs=self._recovery_wait_secs)
    else:
      self._session_manager = session_manager

  def _get_first_op_from_collection(self, key):
    """Returns the first `Operation` from a collection.

    Args:
      key: A string collection key.

    Returns:
      The first Op found in a collection, or `None` if the collection is empty.
    """
    try:
      op_list = ops.get_collection(key)
      if len(op_list) > 1:
        logging.info("Found %d %s operations. Returning the first one.",
                     len(op_list), key)
      if op_list:
        return op_list[0]
    except LookupError:
      pass

    return None

  def _init_ready_op(self, ready_op=USE_DEFAULT):
    """Initializes ready_op.

    Args:
      ready_op: `Operation` to check if the model is initialized.
        If it's set to USE_DEFAULT, creates an op that checks all
        the variables are initialized.
    """
    if ready_op is Supervisor.USE_DEFAULT:
      ready_op = self._get_first_op_from_collection(ops.GraphKeys.READY_OP)
      if ready_op is None:
        ready_op = variables.assert_variables_initialized()
        if ready_op is not None:
          ops.add_to_collection(ops.GraphKeys.READY_OP, ready_op)
    self._ready_op = ready_op

  def _init_init_op(self, init_op=USE_DEFAULT, init_feed_dict=None):
    """Initializes init_op.

    Args:
      init_op: `Operation` to initialize the variables. If set to USE_DEFAULT,
        create an op that initializes all variables and tables.
      init_feed_dict: A dictionary that maps `Tensor` objects to feed values.
        This feed dictionary will be used when `init_op` is evaluated.
    """
    if init_op is Supervisor.USE_DEFAULT:
      init_op = self._get_first_op_from_collection(ops.GraphKeys.INIT_OP)
      if init_op is None:
        init_op = variables.initialize_all_variables()
        ops.add_to_collection(ops.GraphKeys.INIT_OP, init_op)
    self._init_op = init_op
    self._init_feed_dict = init_feed_dict

  def _init_local_init_op(self, local_init_op=USE_DEFAULT):
    """Initializes local_init_op.

    Args:
      local_init_op: `Operation` run for every new supervisor instance. If set
      to USE_DEFAULT create an op based on the `LOCAL_INITIALIZERS` graph
      collection.
    """
    if local_init_op is Supervisor.USE_DEFAULT:
      local_init_op = self._get_first_op_from_collection(
          ops.GraphKeys.LOCAL_INIT_OP)
      if local_init_op is None:
        op_list = [variables.initialize_local_variables(),
                   data_flow_ops.initialize_all_tables()]
        if op_list:
          local_init_op = control_flow_ops.group(*op_list)
          ops.add_to_collection(ops.GraphKeys.LOCAL_INIT_OP, local_init_op)
    self._local_init_op = local_init_op

  def _init_saver(self, saver=USE_DEFAULT):
    """Initializes saver.

    Args:
      saver: A `Saver` object. If set to USE_DEFAULT, create one that
        saves all the variables.
    """
    if saver is Supervisor.USE_DEFAULT:
      saver = self._get_first_op_from_collection(ops.GraphKeys.SAVERS)
      if saver is None and variables.all_variables():
        saver = saver_mod.Saver()
        ops.add_to_collection(ops.GraphKeys.SAVERS, saver)
    self._saver = saver

  def _init_summary_op(self, summary_op=USE_DEFAULT):
    """Initilizes summary_op.

    Args:
      summary_op: An Operation that returns a Summary for the event logs.
        If set to USE_DEFAULT, create an op that merges all the summaries.
    """
    if summary_op is Supervisor.USE_DEFAULT:
      summary_op = self._get_first_op_from_collection(ops.GraphKeys.SUMMARY_OP)
      if summary_op is None:
        summary_op = logging_ops.merge_all_summaries()
        if summary_op is not None:
          ops.add_to_collection(ops.GraphKeys.SUMMARY_OP, summary_op)
    self._summary_op = summary_op

  def _init_global_step(self, global_step=USE_DEFAULT):
    """Initializes global_step.

    Args:
      global_step: An integer Tensor of size 1 that counts steps. If
        set to USE_DEFAULT, creates global_step tensor.
    """
    if global_step is Supervisor.USE_DEFAULT:
      global_step = self._get_first_op_from_collection(
          ops.GraphKeys.GLOBAL_STEP)
      if global_step is None:
        global_step = self._default_global_step_tensor()
        if global_step is not None:
          ops.add_to_collection(ops.GraphKeys.GLOBAL_STEP, global_step)
    self._global_step = global_step

  @property
  def session_manager(self):
    """Return the SessionManager used by the Supervisor.

    Returns:
      A SessionManager object.
    """
    return self._session_manager

  @property
  def coord(self):
    """Return the Coordinator used by the Supervisor.

    The Coordinator can be useful if you want to run multiple threads
    during your training.

    Returns:
      A Coordinator object.
    """
    return self._coord

  @property
  def init_op(self):
    """Return the Init Op used by the supervisor.

    Returns:
      An Op or `None`.
    """
    return self._init_op

  @property
  def init_feed_dict(self):
    """Return the feed dictionary used when evaluating the `init_op`.

    Returns:
      A feed dictionary or `None`.
    """
    return self._init_feed_dict

  @property
  def ready_op(self):
    """Return the Ready Op used by the supervisor.

    Returns:
      An Op or `None`.
    """
    return self._ready_op

  @property
  def summary_writer(self):
    """Return the SummaryWriter used by the supervisor.

    Returns:
      A SummaryWriter.
    """
    return self._summary_writer

  @property
  def summary_op(self):
    """Return the Summary Tensor used by the supervisor.

    Returns:
      A string Tensor for the summary or `None`.
    """
    return self._summary_op

  @property
  def save_summaries_secs(self):
    """Return the delay between summary computations.

    Returns:
      A timestamp.
    """
    return self._save_summaries_secs

  @property
  def global_step(self):
    """Return the global_step Tensor used by the supervisor.

    Returns:
      An integer Tensor for the global_step.
    """
    return self._global_step

  @property
  def saver(self):
    """Return the Saver used by the supervisor.

    Returns:
      A Saver object.
    """
    return self._saver

  @property
  def save_model_secs(self):
    """Return the delay between checkpoints.

    Returns:
      A timestamp.
    """
    return self._save_model_secs

  @property
  def save_path(self):
    """Return the save path used by the supervisor.

    Returns:
      A string.
    """
    return self._save_path

  def _write_graph(self):
    """Writes graph_def to `logdir` and adds it to summary if applicable."""
    if not self._is_chief:
      return
    if self._logdir:
      training_util.write_graph(self._graph.as_graph_def(),
                                self._logdir, "graph.pbtxt")
    if self._summary_writer:
      self._summary_writer.add_graph(self._graph)

  def start_standard_services(self, sess):
    """Start the standard services for 'sess'.

    This starts services in the background.  The services started depend
    on the parameters to the constructor and may include:

      - A Summary thread computing summaries every save_summaries_secs.
      - A Checkpoint thread saving the model every every save_model_secs.
      - A StepCounter thread measure step time.

    Args:
      sess: A Session.

    Returns:
      A list of threads that are running the standard services.  You can use
      the Supervisor's Coordinator to join these threads with:
        sv.coord.Join(<list of threads>)

    Raises:
      ValueError: If not `logdir` was passed to the constructor as the
        services need a log directory.
    """
    if not self._is_chief:
      return
    if not self._logdir:
      logging.warning("Standard services need a 'logdir' "
                      "passed to the SessionManager")
      return

    if self._global_step is not None:
      # Only add the session log if we keep track of global step.
      # TensorBoard cannot use START message for purging expired events
      # if there is no step value.
      current_step = training_util.global_step(sess, self._global_step)
      self._summary_writer.add_session_log(
          SessionLog(status=SessionLog.START),
          current_step)

    threads = []
    if self._summary_op is not None and self._save_summaries_secs:
      threads.append(SVSummaryThread(self, sess))
    if self._global_step is not None and self._save_summaries_secs:
      threads.append(SVStepCounterThread(self, sess))
    if self.saver and self._save_model_secs:
      threads.append(SVTimerCheckpointThread(self, sess))
    for t in threads:
      t.start()
    self._started_threads.extend(threads)

    return threads

  def prepare_or_wait_for_session(self, master="", config=None,
                                  wait_for_checkpoint=False,
                                  max_wait_secs=7200,
                                  start_standard_services=True):
    """Make sure the model is ready to be used.

    Create a session on 'master', recovering or initializing the model as
    needed, or wait for a session to be ready.  If running as the chief
    and `start_standard_service` is set to True, also call the session
    manager to start the standard services.

    Args:
      master: name of the TensorFlow `master` to use.  If not specified or
        empty a 'Direct Session' is created.
      config: Optional ConfigProto proto used to configure the session,
        which is passed as-is to create the session.
      wait_for_checkpoint: Whether we should wait for the availability of a
        checkpoint before creating Session. Defaults to False.
      max_wait_secs: Maximum time to wait for the session to become available.
      start_standard_services: Whether to start the standard services,
        such as checkpoint, summary and step counter.

    Returns:
      A Session object that can be used to drive the model.
    """
    if self._is_chief:
      sess = self._session_manager.prepare_session(
          master, self.init_op, self.saver, self._logdir,
          wait_for_checkpoint=wait_for_checkpoint, max_wait_secs=max_wait_secs,
          config=config, init_feed_dict=self._init_feed_dict)
      self._write_graph()
      # For users who recreate the session with
      # prepare_or_wait_for_session(), we need to clear the
      # coordinator's stop_event so that threads managed by the
      # coordinator can run.
      self._coord.clear_stop()
      if start_standard_services:
        self.start_standard_services(sess)
    else:
      sess = self._session_manager.wait_for_session(master,
                                                    config=config,
                                                    max_wait_secs=max_wait_secs)

    return sess

  def start_queue_runners(self, sess, queue_runners=None):
    """Start threads for `QueueRunners`.

    Args:
      sess: A `Session`.
      queue_runners: A list of `QueueRunners`. If not specified, we'll use the
        list of queue runners gathered in the graph under the key
        `GraphKeys.QUEUE_RUNNERS`.

    Returns:
      The list of threads started for the `QueueRunners`.
    """
    if queue_runners is None:
      queue_runners = self._graph.get_collection(ops.GraphKeys.QUEUE_RUNNERS)
    threads = []
    for qr in queue_runners:
      threads.extend(qr.create_threads(sess, coord=self._coord, daemon=True,
                                       start=True))
    self._started_threads.extend(threads)
    return threads

  def loop(self, timer_interval_secs, target, args=None):
    """Start a LooperThread that calls a function periodically.

    If `timer_interval_secs` is None the thread calls `target(args)`
    repeatedly.  Otherwise `target(args)` is called every `timer_interval_secs`
    seconds.  The thread terminates when a stop is requested.

    The started thread is added to the list of threads managed by the supervisor
    so it does not need to be passed to the `stop()` method.

    Args:
      timer_interval_secs: Number. Time boundaries at which to call `target`.
      target: A callable object.
      args: Optional arguments to pass to `target` when calling it.

    Returns:
      The started thread.
    """
    looper = coordinator.LooperThread(self._coord, timer_interval_secs,
                                      target=target, args=args)
    looper.start()
    self._started_threads.append(looper)
    return looper

  def stop(self, threads=None, close_summary_writer=True):
    """Stop the services and the coordinator.

    This does not close the session.

    Args:
      threads: Optional list of threads to join with the coordinator.  If
        `None`, defaults to the threads running the standard services plus the
        threads started for `QueueRunners` if `start_queue_runners()` was
        called.  To wait on an additional set of threads, pass the list in this
        parameter and they will be merged with the internal list of running
        services.
      close_summary_writer: Whether to close the `summary_writer`.  Defaults to
        `True`.
    """
    join_threads = []
    join_threads.extend(self._started_threads)
    if threads is not None:
      join_threads.extend(threads)
    self._coord.request_stop()
    self._coord.join(join_threads)

    # Close the write last, in case one of the running threads was using it.
    if close_summary_writer and self._summary_writer:
      # Stop messages are not logged with event.step,
      # since the session may have already terminated.
      self._summary_writer.add_session_log(SessionLog(status=SessionLog.STOP))
      self._summary_writer.close()

    self._started_threads = []

  def request_stop(self, ex=None):
    """Request that the coordinator stop the threads.

    See `Coordinator.request_stop()`.

    Args:
      ex: Optional `Exception`, or Python `exc_info` tuple as returned by
        `sys.exc_info()`.  If this is the first call to `request_stop()` the
        corresponding exception is recorded and re-raised from `join()`.
    """
    self._coord.request_stop(ex=ex)

  def should_stop(self):
    """Check if the coordinator was told to stop.

    See `Coordinator.should_stop()`.

    Returns:
      True if the coordinator was told to stop, False otherwise.
    """
    return self._coord.should_stop()

  def stop_on_exception(self):
    """Context handler to stop the supervisor when an exception is raised.

    See `Coordinator.stop_on_exception()`.

    Returns:
      A context handler.
    """
    return self._coord.stop_on_exception()

  def wait_for_stop(self):
    """Block waiting for the coordinator to stop."""
    self._coord.wait_for_stop()

  def summary_computed(self, sess, summary, global_step=None):
    """Indicate that a summary was computed.

    Args:
      sess: A `Session` object.
      summary: A Summary proto, or a string holding a serialized summary proto.
      global_step: Int. global step this summary is associated with. If `None`,
        it will try to fetch the current step.

    Raises:
      TypeError: if 'summary' is not a Summary proto or a string.
      RuntimeError: if the Supervisor was created without a `logdir`.
    """
    if not self._logdir:
      raise RuntimeError("summary_computed() requires a logdir")
    if global_step is None and self.global_step is not None:
      global_step = training_util.global_step(sess, self.global_step)
    if self._summary_writer:
      self._summary_writer.add_summary(summary, global_step)

  def _default_global_step_tensor(self):
    try:
      gs = ops.get_default_graph().get_tensor_by_name("global_step:0")
      if gs.dtype.base_dtype in [dtypes.int32, dtypes.int64]:
        return gs
      else:
        logging.warning("Found 'global_step' is not an int type: %s", gs.dtype)
        return None
    except KeyError:
      return None

  def _verify_setup(self):
    """Check that all is good.

    Raises:
      ValueError: If something is not good.
    """
    # Not running as chief means that replicas are used.
    # In that case all Variables must have their device set.
    if not self._is_chief:
      for op in self._graph.get_operations():
        if op.type == "Variable" and not op.device:
          raise ValueError("When using replicas, all Variables must have "
                           "their device set: %s" % op)


class SVSummaryThread(coordinator.LooperThread):
  """A thread to save summaries on a timer."""

  def __init__(self, sv, sess):
    """Create a SVSummaryThread.

    Args:
      sv: A `Supervisor`.
      sess: A `Session`.
    """
    super(SVSummaryThread, self).__init__(sv.coord, sv.save_summaries_secs)
    self._sv = sv
    self._sess = sess

  def run_loop(self):
    if self._sv.global_step is not None:
      summary_strs, global_step = self._sess.run([self._sv.summary_op,
                                                  self._sv.global_step])
    else:
      summary_strs = self._sess.run(self._sv.summary_op)
      global_step = None
    if self._sv.summary_writer:
      self._sv.summary_writer.add_summary(summary_strs, global_step)


class SVStepCounterThread(coordinator.LooperThread):
  """Threads to count steps and measure their duration."""

  def __init__(self, sv, sess):
    """Create a `SVStepCounterThread`.

    Args:
      sv: A `Supervisor`.
      sess: A `Session`.
    """
    super(SVStepCounterThread, self).__init__(sv.coord, sv.save_summaries_secs)
    self._sv = sv
    self._sess = sess
    self._last_time = 0.0
    self._last_step = 0
    self._summary_tag = "%s/sec" % self._sv.global_step.op.name

  def start_loop(self):
    self._last_time = time.time()
    self._last_step = training_util.global_step(
        self._sess, self._sv.global_step)

  def run_loop(self):
    # Count the steps.
    current_step = training_util.global_step(self._sess, self._sv.global_step)
    added_steps = current_step - self._last_step
    self._last_step = current_step
    # Measure the elapsed time.
    current_time = time.time()
    elapsed_time = current_time - self._last_time
    self._last_time = current_time
    # Reports the number of steps done per second
    steps_per_sec = added_steps / elapsed_time
    summary = Summary(value=[Summary.Value(tag=self._summary_tag,
                                           simple_value=steps_per_sec)])
    if self._sv.summary_writer:
      self._sv.summary_writer.add_summary(summary, current_step)
    logging.log_first_n(logging.INFO, "%s: %g", 10,
                        self._summary_tag, steps_per_sec)


class SVTimerCheckpointThread(coordinator.LooperThread):
  """A thread to checkpoint on a timer."""

  def __init__(self, sv, sess):
    """Create a `SVTimerCheckpointThread`.

    Args:
      sv: A `Supervisor`.
      sess: A `Session`.
    """
    super(SVTimerCheckpointThread, self).__init__(sv.coord, sv.save_model_secs)
    self._sv = sv
    self._sess = sess

  def run_loop(self):
    self._sv.saver.save(self._sess, self._sv.save_path,
                        global_step=self._sv.global_step)
    if self._sv.summary_writer and self._sv.global_step is not None:
      current_step = training_util.global_step(self._sess, self._sv.global_step)
      self._sv.summary_writer.add_session_log(
          SessionLog(status=SessionLog.CHECKPOINT,
                     checkpoint_path=self._sv.save_path),
          current_step)


# TODO(sherrym): All non-PEP8 compliant names will be deprecated shortly.
setattr(Supervisor, "PrepareSession", Supervisor.prepare_or_wait_for_session)
setattr(Supervisor, "StartQueueRunners", Supervisor.start_queue_runners)
setattr(Supervisor, "StartStandardServices", Supervisor.start_standard_services)
setattr(Supervisor, "Stop", Supervisor.stop)
setattr(Supervisor, "RequestStop", Supervisor.request_stop)
setattr(Supervisor, "Loop", Supervisor.loop)
setattr(Supervisor, "ShouldStop", Supervisor.should_stop)
setattr(Supervisor, "StopOnException", Supervisor.stop_on_exception)
setattr(Supervisor, "WaitForStop", Supervisor.wait_for_stop)
setattr(Supervisor, "SummaryComputed", Supervisor.summary_computed)
