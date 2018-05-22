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
# ======================================

"""Library of TPU helper functions."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from six.moves import xrange  # pylint: disable=redefined-builtin

from tensorflow.contrib.tpu.python.ops import tpu_ops
from tensorflow.contrib.tpu.python.tpu import tpu_function

from tensorflow.core.framework import attr_value_pb2
from tensorflow.python.framework import device as pydev
from tensorflow.python.framework import errors
from tensorflow.python.framework import ops
from tensorflow.python.ops import array_ops
from tensorflow.python.ops import control_flow_ops
from tensorflow.python.ops import variable_scope
from tensorflow.python.platform import tf_logging as logging
from tensorflow.python.util import compat


# Operations that indicate some error in the users graph, e.g. a placeholder
# that's introduced outside of the infeed.
_BLACKLISTED_OPS = set([
    "Placeholder",
])

# These operations will currently fail to compile, but we should be able to
# support them eventually via CPU offload or extending our operation set.
_NOT_IMPLEMENTED_OPS = set([
    "AudioSummary",
    "AudioSummaryV2",
    "HistogramSummary",
    "ImageSummary",
    "MergeSummary",
    "Print",
    "ScalarSummary",
    "TensorSummary",
    "TensorSummaryV2",
    ])

_MAX_WARNING_LINES = 5

_TPU_REPLICATE_ATTR = "_tpu_replicate"
_TPU_COMPILATION_STATUS_ATTR = "_tpu_compilation_status"
_OUTSIDE_COMPILATION_ATTR = "_xla_outside_compilation"


def _tpu_system_device_name(job):
  """Returns the device name for the TPU_SYSTEM device of `job`."""
  if job is None:
    return "/device:TPU_SYSTEM:0"
  else:
    return "/job:%s/device:TPU_SYSTEM:0" % job


def initialize_system(embedding_config=None, job=None):
  """Initializes a distributed TPU system for use with TensorFlow.

  Args:
    embedding_config: If not None, an `EmbeddingLayerConfiguration` proto
      describing the desired configuration of the hardware embedding lookup
      tables. If embedding_config is None, no hardware embeddings can be used.
    job: The job (the XXX in TensorFlow device specification /job:XXX)
      that contains the TPU devices that will be initialized. If job=None
      it is assumed there is only one job in the TensorFlow flock, and an
      error will be returned if this assumption does not hold.
  Returns:
    A serialized `TopologyProto` that describes the TPU system. Note:
      the topology must be evaluated using `Session.run` before it can be used.
  """
  config_string = ("" if embedding_config is None else
                   embedding_config.SerializeToString())
  with ops.device(_tpu_system_device_name(job)):
    return tpu_ops.configure_distributed_tpu(embedding_config=config_string)


def shutdown_system(job=None):
  """Shuts down a running a distributed TPU system."""
  with ops.device(_tpu_system_device_name(job)):
    shutdown_distributed_tpu = tpu_ops.shutdown_distributed_tpu()
  return shutdown_distributed_tpu


def core(num):
  """Returns the device name for a core in a replicated TPU computation.

  Args:
    num: the virtual core number within each replica to which operators should
    be assigned.
  Returns:
    A device name, suitable for passing to `tf.device()`.
  """
  return "device:TPU_REPLICATED_CORE:{}".format(num)


class TPUReplicateContext(control_flow_ops.XLAControlFlowContext):
  """A `ControlFlowContext` for nodes inside a TPU computation.

  The primary role of `TPUReplicateContext` is to mark operators inside a
  tpu.replicate() computation with the attribute "_tpu_replicate=XYZ", where XYZ
  is a unique name.

  We use a `ControlFlowContext` to perform the annotation since it
  integrates with Tensorflow constructs like ResourceVariables. For example,
  if a `ResourceVariable` is constructed inside a tpu.replicate() block, the
  `ResourceVariable` implementation can use
  `with ops.control_dependencies(None)` to build the variable's definition
  outside the replicated computation.
  """

  def __init__(self, name, num_replicas):
    super(TPUReplicateContext, self).__init__()
    self._num_replicas = num_replicas
    self._outer_device_function_stack = None
    self._oc_dev_fn_stack = None
    self._outside_compilation_cluster = None
    self._outside_compilation_counter = 0
    self._in_gradient_colocation = None
    self._gradient_colocation_stack = []
    self._host_compute_core = []
    self._name = name
    self._unsupported_ops = []

  def report_unsupported_operations(self):
    if self._unsupported_ops:
      op_str = "\n".join(["  %s (%s)" % (op.type, op.name)
                          for op in self._unsupported_ops[:_MAX_WARNING_LINES]])
      logging.warning("%d unsupported operations found: \n%s",
                      len(self._unsupported_ops), op_str)
      if len(self._unsupported_ops) > _MAX_WARNING_LINES:
        logging.warning("... and %d more" %
                        (len(self._unsupported_ops) - _MAX_WARNING_LINES))

  def EnterGradientColocation(self, op, gradient_uid):
    if op is not None:
      self._gradient_colocation_stack.append(op)
      if not self._outside_compilation_cluster:
        try:
          outside_attr = op.get_attr(_OUTSIDE_COMPILATION_ATTR)
          if self._in_gradient_colocation:
            raise NotImplementedError(
                "Cannot nest gradient colocation operations outside compilation"
            )
          if gradient_uid == "__unsupported__":
            raise NotImplementedError(
                "No gradient_uid calling gradient within outside_compilation")
          # When we take the gradient of an op X in an
          # outside_compilation cluster C in a forward computation we
          # would like to put the ops corresponding to the gradient of
          # X into a new outside_compilation cluster C'. However, if
          # we take the gradient of X twice, the second one should get
          # yet another new outside_compilation cluster C''.
          #
          # The mechanism we adopt is to use a 'root_cluster' which is
          # the cluster that X was in before we took gradients, and a
          # 'gradient_uid' which is different for every invocation of
          # gradients, and put the gradient of X in cluster
          # 'root_cluster.gradient_uid'.
          #
          # When taking a gradient of a gradient, some ops will be
          # colocated with Op in the forward pass (e.g., cluster
          # root_cluster) and some in the backward pass (e.g., cluster
          # root_cluster.initial_gradient_uid). We need all of the
          # grad-of-grad ops to be in the same cluster to avoid cyclic
          # dependencies between clusters. We adopt a heuristic that
          # puts any op clustered with root_cluster.<xxx> in
          # root_cluster.gradient_uid, even if xxx was
          # initial_gradient_uid.
          self._in_gradient_colocation = op
          parts = outside_attr.split(".")
          cluster = parts[0] + "." + gradient_uid
          self._EnterOutsideCompilationScope(cluster=cluster)
        except ValueError:
          # The attr was not present: do nothing.
          pass

  def ExitGradientColocation(self, op, gradient_uid):
    if op is not None:
      if not self._gradient_colocation_stack:
        raise errors.InternalError(
            op.node_def, op,
            "Badly nested gradient colocation: empty stack when popping Op " +
            op.name)
      last_op = self._gradient_colocation_stack.pop()
      if op is last_op:
        if op is self._in_gradient_colocation:
          self._in_gradient_colocation = None
          self._ExitOutsideCompilationScope()
      else:
        raise errors.InternalError(
            op.node_def, op, "Badly nested gradient colocation, expected " +
            last_op + ", got " + op.name)

  def _EnterOutsideCompilationScope(self, cluster=None):

    class FakeOp(object):
      """A helper class to determine the current device.

      Supports only the device set/get methods needed to run the
      graph's _apply_device_function method.
      """

      def __init__(self):
        self._device = ""

      @property
      def device(self):
        return self._device

      def _set_device(self, device):
        self._device = device.to_string()

    if self._outside_compilation_cluster:
      raise NotImplementedError("Cannot nest outside_compilation clusters")
    if cluster:
      self._outside_compilation_cluster = cluster
    else:
      self._outside_compilation_cluster = str(self._outside_compilation_counter)
      self._outside_compilation_counter += 1
    graph = ops.get_default_graph()
    fake_op = FakeOp()
    graph._apply_device_functions(fake_op)  # pylint: disable=protected-access
    device = pydev.DeviceSpec.from_string(fake_op.device)
    if (device.device_type == "TPU_REPLICATED_CORE" and
        device.device_index is not None):
      self._host_compute_core.append(self._outside_compilation_cluster + ":" +
                                     str(device.device_index))
    self._oc_dev_fn_stack = graph._device_function_stack  # pylint: disable=protected-access
    graph._device_function_stack = self._outer_device_function_stack  # pylint: disable=protected-access

  def _ExitOutsideCompilationScope(self):
    if not self._outside_compilation_cluster:
      raise NotImplementedError(
          "Attempted to exit outside_compilation scope when not in scope")
    self._outside_compilation_cluster = None
    graph = ops.get_default_graph()
    graph._device_function_stack = self._oc_dev_fn_stack  # pylint: disable=protected-access

  def Enter(self):
    if not self._outer_device_function_stack:
      # Capture the device function stack at the time of first entry
      # since that is the stack that will be used outside_compilation.
      graph = ops.get_default_graph()
      self._outer_device_function_stack = list(graph._device_function_stack)  # pylint: disable=protected-access
    super(TPUReplicateContext, self).Enter()

  def Exit(self):
    super(TPUReplicateContext, self).Exit()

  def HostComputeCore(self):
    return self._host_compute_core

  def AddOp(self, op):
    self._AddOpInternal(op)

  def _AddOpInternal(self, op):
    # pylint: disable=protected-access
    if op.type in _BLACKLISTED_OPS:
      logging.error("Operation of type %s (%s) is not supported on the TPU. "
                    "Execution will fail if this op is used in the graph. " %
                    (op.type, op.name))

    if op.type in _NOT_IMPLEMENTED_OPS:
      self._unsupported_ops.append(op)

    if any(x.dtype._is_ref_dtype for x in op.inputs):
      raise NotImplementedError(
          "Non-resource Variables are not supported inside TPU computations "
          "(operator name: %s)" % op.name)
    if _TPU_REPLICATE_ATTR in op.node_def.attr:
      raise ValueError("TPU computations cannot be nested")
    op._set_attr(_TPU_REPLICATE_ATTR,
                 attr_value_pb2.AttrValue(s=compat.as_bytes(self._name)))
    if self._outside_compilation_cluster:
      op._set_attr(
          _OUTSIDE_COMPILATION_ATTR,
          attr_value_pb2.AttrValue(
              s=compat.as_bytes(self._outside_compilation_cluster)))
    if self._num_replicas > 1 or not self._outside_compilation_cluster:
      # Prevent feeding or fetching anything that is being compiled,
      # and any replicated outside_compilation Op.
      op.graph.prevent_feeding(op)
      op.graph.prevent_fetching(op)

  def AddValue(self, val):
    result = val
    if self._outer_context:
      result = self._outer_context.AddValue(val)
    return result

  def AddInnerOp(self, op):
    self._AddOpInternal(op)
    if self._outer_context:
      self._outer_context.AddInnerOp(op)

  @property
  def grad_state(self):
    # Define the gradient loop state associated with the TPUReplicateContext to
    # be None as the TPUReplicateContext does not get nested nor does the
    # grad_state outside the TPUReplicateContext affect the graph inside so the
    # grad_state should be as if this is the top-level gradient state.
    return None


def outside_compilation(computation, args=None):
  """Builds part of a computation outside any current TPU replicate scope.

  Args:
    computation: A Python function that builds the computation to
      place on the host.
    args: Inputs to pass to computation.
  Returns:
    The Tensors returned by computation.
  """
  graph = ops.get_default_graph()

  # If we are in a TPUReplicateContext, signal that we are now
  # outside_compilation
  initial_context = graph._get_control_flow_context()  # pylint: disable=protected-access
  context = initial_context
  while context:
    if isinstance(context, TPUReplicateContext):
      context._EnterOutsideCompilationScope()  # pylint: disable=protected-access
    context = context.outer_context

  retval = computation(*args)

  # If we are in a TPUReplicateContext, signal that we are no longer
  # outside_compilation
  final_context = graph._get_control_flow_context()  # pylint: disable=protected-access
  if initial_context is not final_context:
    raise NotImplementedError(
        "Control-flow context cannot be different at start and end of an "
        "outside_compilation scope")
  context = initial_context
  while context:
    if isinstance(context, TPUReplicateContext):
      context._ExitOutsideCompilationScope()  # pylint: disable=protected-access
    context = context.outer_context

  return retval


def replicate(computation,
              inputs=None,
              infeed_queue=None,
              device_assignment=None,
              name=None):
  """Builds a graph operator that runs a replicated TPU computation.

  Args:
    computation: A Python function that builds the computation to replicate.
    inputs: A list of lists of input tensors or `None` (equivalent to
      `[[]]`), indexed by `[replica_num][input_num]`. All replicas must
      have the same number of inputs.
    infeed_queue: If not `None`, the `InfeedQueue` from which to append a tuple
      of arguments as inputs to computation.
    device_assignment: If not `None`, a `DeviceAssignment` describing the
      mapping between logical cores in the computation with physical cores in
      the TPU topology. Uses a default device assignment if `None`. The
      `DeviceAssignment` may be omitted if each replica of the computation uses
      only one core, and there is either only one replica, or the number of
      replicas is equal to the number of cores in the TPU system.
    name: (Deprecated) Does nothing.
  Returns:
    A list of lists of output tensors, indexed by `[replica_num][output_num]`.
  Raises:
    ValueError: If all replicas do not have equal numbers of input tensors.
    ValueError: If the number of inputs per replica does not match
      the number of formal parameters to `computation`.
  """
  return split_compile_and_replicate(computation, inputs, infeed_queue,
                                     device_assignment, name)[1]


def split_compile_and_replicate(computation,
                                inputs=None,
                                infeed_queue=None,
                                device_assignment=None,
                                name=None,
                                use_tpu=True):
  """Builds graph operators that runs compilation and replicated computation.

  This is a lower level interface than replicate that returns a separate compile
  and execute output tensor. In the generated graph the compile op feeds into
  the execute op and no additional compilation is incurred when running the
  compile op before the execute op. The compile op returns additional
  information about the compilation but does not return the compiled program.

  Args:
    computation: A Python function that builds the computation to replicate.
    inputs: A list of lists of input tensors or `None` (equivalent to
      `[[]]`), indexed by `[replica_num][input_num]`. All replicas must
      have the same number of inputs.
    infeed_queue: If not `None`, the `InfeedQueue` from which to append a tuple
      of arguments as inputs to computation.
    device_assignment: If not `None`, a `DeviceAssignment` describing the
      mapping between logical cores in the computation with physical cores in
      the TPU topology. Uses a default device assignment if `None`. The
      `DeviceAssignment` may be omitted if each replica of the computation uses
      only one core, and there is either only one replica, or the number of
      replicas is equal to the number of cores in the TPU system.
    name: (Deprecated) Does nothing.
    use_tpu: When false, the input `computation` is executed on the XLA CPU/GPU
      backends. Currently, only supports a default placement (computation is
      placed on GPU if one is available, and on CPU if not).
  Returns:
    A list of lists with the first list corresponding to the compile op and the
    second a list of output tensors, indexed by `[replica_num][output_num]`.
  Raises:
    ValueError: If all replicas do not have equal numbers of input tensors.
    ValueError: If the number of inputs per replica does not match
      the number of formal parameters to `computation`.
  """
  del name
  inputs = [[]] if inputs is None else inputs

  metadata_kwargs = {}
  if device_assignment is not None:
    # Turn the Numpy array into a flattened list so we can pass it as an
    # operator attribute.
    metadata_kwargs = {
        "topology":
            device_assignment.topology.serialized(),
        "device_assignment":
            device_assignment.core_assignment.flatten().tolist(),
        "computation_shape":
            device_assignment.computation_shape.tolist()
    }

  if ((not isinstance(inputs, list)) or
      any(not isinstance(inp, (list, tuple)) for inp in inputs)):
    raise TypeError("tpu.replicate() inputs must be a list of lists/tuples")

  num_replicas = len(inputs)

  # No replicas? Nothing to do.
  if num_replicas == 0:
    return []

  # Converts inputs to Tensors.
  inputs = [[ops.convert_to_tensor(x) for x in inp] for inp in inputs]

  # Verifies that all replicas have matching numbers and types of inputs
  input_types = [x.dtype for x in inputs[0]]
  input_arity = len(input_types)
  for i in range(num_replicas):
    if len(inputs[i]) != input_arity:
      raise ValueError("Replicas must have the same number of inputs. "
                       "Replica 0 had {} inputs, replica {} had {} "
                       "inputs.".format(input_arity, i, len(inputs[i])))

    types = [x.dtype for x in inputs[i]]
    if types != input_types:
      raise ValueError(
          "Replicas must have matching input types. Replica 0 had "
          "input types {}, replica {} had input types {}".format(
              input_types, i, types))

  arg_error = tpu_function.check_function_argument_count(
      computation, input_arity, infeed_queue)
  if arg_error is not None:
    if infeed_queue is None:
      raise TypeError(
          "Supplied computation cannot be called with the specified inputs. "
          "You specified %d inputs: %s, but the computation needs %s" % (
              input_arity, str([i.name for i in inputs[0]]), arg_error))
    else:
      raise TypeError(
          "Supplied computation cannot be called with the specified inputs. "
          "You specified %d inputs: %s and %d additional inputs from infeed,"
          " but the computation needs %s" % (input_arity, str(
              [i.name
               for i in inputs[0]]), infeed_queue.number_of_tuple_elements,
                                             arg_error))

  graph = ops.get_default_graph()

  # Fan-in: Builds a TPUReplicatedInput node for each input.
  computation_inputs = []
  for i in range(0, input_arity):
    replicas = [inputs[replica][i] for replica in xrange(num_replicas)]
    computation_inputs.append(
        tpu_ops.tpu_replicated_input(replicas, name="input{}".format(i)))

  cluster_name = graph.unique_name("cluster")
  context = TPUReplicateContext(name=cluster_name, num_replicas=num_replicas)
  try:
    context.Enter()

    metadata = tpu_ops.tpu_replicate_metadata(
        num_replicas=num_replicas, use_tpu=use_tpu, **metadata_kwargs)

    with tpu_function.tpu_shard_context(
        num_replicas), ops.control_dependencies([metadata]):

      # The EncapsulateTPUComputations rewrite needs to identify the
      # replicated arguments inside each computation. Adds identity operators
      # tagged with an attribute _tpu_replicated_input to identify the
      # replicated inputs.
      # pylint: disable=protected-access
      with graph._attr_scope({"_tpu_replicated_input":
                              attr_value_pb2.AttrValue(b=True)}):
        computation_inputs = [
            array_ops.identity(x, name="replicated_input_{}".format(i))
            for i, x in enumerate(computation_inputs)]
      # pylint: enable=protected-access

      # If there is an infeed queue, adds the dequeued values to the
      # computation's inputs.
      if infeed_queue is not None:
        infeed_queue.set_number_of_shards(num_replicas)
        for t in infeed_queue.generate_dequeue_op():
          computation_inputs.append(t)

      # Only resource variables work inside a TPU computation, so turn on
      # resource variables for the computation.
      # TODO(phawkins): consider removing this code. It will
      # be less confusing to clients if they knowingly choose to use resource
      # variables.
      vscope = variable_scope.get_variable_scope()
      saved_use_resource = vscope.use_resource
      vscope.set_use_resource(True)

      outputs = computation(*computation_inputs)

      vscope.set_use_resource(saved_use_resource)

    # If the computation only returned one value, makes it a tuple.
    if not isinstance(outputs, (list, tuple)):
      outputs = (outputs,)

    try:
      with ops.device(core(0)):
        outputs = [
            o if isinstance(o, ops.Operation) else ops.convert_to_tensor(o)
            for o in outputs
        ]
    except Exception as e:
      raise ValueError(
          "TPU function return values must all either be Operations or "
          "convertible to Tensors. Got '%s'" % str(e))

    # Separates the returned Operations and Tensors.
    output_operations = [o for o in outputs if isinstance(o, ops.Operation)]
    output_tensors = [o for o in outputs if not isinstance(o, ops.Operation)]

    if outputs != output_tensors + output_operations:
      raise ValueError(
          "TPU functions must return zero-or more Tensor values followed by "
          "zero or more Operations.")
    output_arity = len(output_tensors)

    # Wraps outputs in Identity ops. Otherwise a replicated input copied
    # straight to an output would bypass the replicate(). This would be bad
    # because the TPUReplicatedInput/TPUReplicatedOutput operator would not
    # be rewritten away, leading to a runtime error.
    # TODO(phawkins): extend the rewrite to elide these nodes instead.
    new_output_tensors = []
    for t in output_tensors:
      with ops.device(t.device if t.device else core(0)):
        new_output_tensors.append(array_ops.identity(t))
    output_tensors = new_output_tensors
  finally:
    context.report_unsupported_operations()
    context.Exit()
    host_compute_core = context.HostComputeCore()

  if host_compute_core:
    attr_value = attr_value_pb2.AttrValue()
    attr_value.list.s.extend([compat.as_bytes(x) for x in host_compute_core])
    metadata._set_attr("host_compute_core", attr_value)  # pylint: disable=protected-access

  # Fan-out: Builds a TPUReplicatedOutput node for each output.
  outputs = [tpu_ops.tpu_replicated_output(output_tensors[i], num_replicas,
                                           name="output{}".format(i))
             for i in xrange(output_arity)]

  with ops.control_dependencies([metadata]):
    if use_tpu:
      compile_status = tpu_ops.tpu_compilation_result()
      op = compile_status.op
      attr_value = attr_value_pb2.AttrValue(s=compat.as_bytes(cluster_name))
      op._set_attr(_TPU_COMPILATION_STATUS_ATTR, attr_value)  # pylint: disable=protected-access
    else:
      compile_status = control_flow_ops.no_op(name="compilation_status")

  with ops.control_dependencies(output_operations):
    if output_arity == 0:
      # Returns a list of NoOps dependent on the replication Op, indexed by
      # [replica_num].
      return [
          compile_status, [
              control_flow_ops.no_op(name="shard_%d" % i)
              for i in range(num_replicas)
          ]
      ]
    else:
      # Wraps the outputs in identity operators so the names of any possible
      # `fetch` nodes are preserved by the replication rewrite.
      return [
          compile_status, [[
              array_ops.identity(
                  outputs[out][replica],
                  name="output_%d_shard_%d" % (out, replica))
              for out in xrange(output_arity)
          ]
                           for replica in xrange(num_replicas)]
      ]


def shard(computation,
          inputs=None,
          num_shards=1,
          input_shard_axes=None,
          outputs_from_all_shards=True,
          output_shard_axes=None,
          infeed_queue=None,
          device_assignment=None,
          name=None):
  """Shards `computation` for parallel execution.

  `inputs` must be a list of Tensors or None (equivalent to an empty
  list), each of which has a corresponding split axis (from
  `input_shard_axes`). Each input is split into `num_shards` pieces
  along the corresponding axis, and computation is applied to each
  shard in parallel.

  Tensors are broadcast to all shards if they are lexically captured by
  `computation`. e.g.,

  x = tf.constant(7)
  def computation():
    return x + 3
  ... = shard(computation, ...)

  TODO(phawkins): consider adding support for broadcasting Tensors passed
  as inputs.

  If `outputs_from_all_shards` is true, the outputs from all shards of
  `computation` are concatenated back together along their `output_shards_axes`.
  Otherwise, each output is taken from an arbitrary shard.

  Inputs and outputs of the computation must be at least rank-1 Tensors.

  Args:
    computation: A Python function that builds a computation to apply to each
      shard of the input.
    inputs: A list of input tensors or None (equivalent to an empty
      list). Each input tensor has a corresponding shard axes, given
      by `input_shard_axes`, which must have size divisible by
      `num_shards`.
    num_shards: The number of shards.
    input_shard_axes: A list of dimensions along which to shard `inputs`, or
      `None`. `None` means "shard all inputs along dimension 0". If not `None`,
      there must be one dimension per input.
    outputs_from_all_shards: Boolean or list of boolean. For each output, if
      `True`, outputs from all shards are concatenated along the corresponding
      `output_shard_axes` entry. Otherwise, each output is taken
      from an arbitrary shard. If the argument is a boolean, the argument's
      value is used for each output.
    output_shard_axes: A list of dimensions along which to concatenate the
      outputs of `computation`, or `None`. `None` means "concatenate all outputs
      along dimension 0". If not `None`, there must be one dimension per output.
      Ignored if `outputs_from_all_shards` is False.
    infeed_queue: If not `None`, the `InfeedQueue` to use to augment the inputs
      of `computation`.
    device_assignment: If not `None`, a `DeviceAssignment` describing the
      mapping between logical cores in the computation with physical cores in
      the TPU topology. Uses a default device assignment if `None`. The
      `DeviceAssignment` may be omitted if each shard of the computation uses
      only one core, and there is either only one shard, or the number of shards
      is equal to the number of cores in the TPU system.
    name: (Deprecated) Does nothing.
  Returns:
    A list of output tensors.
  Raises:
    ValueError: If num_shards <= 0
    ValueError: If len(input_shard_axes) != len(inputs)
    ValueError: If len(output_shard_axes) != len(outputs from `computation`)
  """

  if num_shards <= 0:
    raise ValueError("num_shards must be a positive integer.")

  # Converts inputs to Tensors.
  inputs = [] if inputs is None else [ops.convert_to_tensor(x) for x in inputs]

  if input_shard_axes is None:
    input_shard_axes = [0] * len(inputs)
  if len(inputs) != len(input_shard_axes):
    raise ValueError("Length of input_shard_axes must be equal to the number "
                     "of inputs.")

  if inputs:
    # Splits the `inputs` along the corresponding `input_shard_axes`, giving
    # lists with layout [input][shard]
    split_inputs = [
        array_ops.split(x, num_shards, axis=axis)
        for (axis, x) in zip(input_shard_axes, inputs)]

    # Transposes the input lists to have layout [shard][input]
    transposed_inputs = [list(i) for i in zip(*split_inputs)]
  else:
    transposed_inputs = [[]] * num_shards

  outputs = replicate(
      computation,
      transposed_inputs,
      infeed_queue=infeed_queue,
      device_assignment=device_assignment,
      name=name)

  # There must be at least one shard since num_shards > 0.
  # TODO(b/36647078) remove disable when pylint bug is fixed.
  # pylint: disable=indexing-exception
  if isinstance(outputs[0], ops.Operation):
    # pylint: enable=indexing-exception
    # There were no outputs from the computation and replicate returned a list
    # of NoOps with control dependencies on the computation. Return the first
    # one so it can be used as a control dependency or fetch node.
    # TODO(b/36647078) remove disable when pylint bug is fixed.
    # pylint: disable=indexing-exception
    return [outputs[0]]
    # pylint: enable=indexing-exception

  # TODO(b/36647078) remove disable when pylint bug is fixed.
  # pylint: disable=indexing-exception
  num_outputs = len(outputs[0])
  # pylint: enable=indexing-exception

  if output_shard_axes is None:
    output_shard_axes = [0] * num_outputs
  if num_outputs != len(output_shard_axes):
    raise ValueError("Length of output_shard_axes must be equal to the number "
                     "of outputs.")

  if isinstance(outputs_from_all_shards, bool):
    outputs_from_all_shards = [outputs_from_all_shards] * num_outputs

  if num_outputs != len(outputs_from_all_shards):
    raise ValueError("Length of outputs_from_all_shards must be equal to the "
                     "number of outputs.")

  results = []
  for (axis, all_shards, x) in zip(output_shard_axes, outputs_from_all_shards,
                                   zip(*outputs)):
    if all_shards:
      # Concatenate all of the outputs together (use stack for scalars).
      shape = x[0].shape
      is_scalar = shape is not None and (shape.ndims == 0)
      results.append((array_ops.stack(list(x)) if is_scalar
                      else array_ops.concat(list(x), axis=axis)))
    else:
      # TODO(phawkins): use a smarter policy, e.g., round-robin across shards.
      results.append(x[0])

  return results


def batch_parallel(computation,
                   inputs=None,
                   num_shards=1,
                   infeed_queue=None,
                   device_assignment=None,
                   name=None):
  """Shards `computation` along the batch dimension for parallel execution.

  Convenience wrapper around shard().

  `inputs` must be a list of Tensors or None (equivalent to an empty
  list). Each input is split into `num_shards` pieces along the 0-th
  dimension, and computation is applied to each shard in parallel.

  Tensors are broadcast to all shards if they are lexically captured by
  `computation`. e.g.,

  x = tf.constant(7)
  def computation():
    return x + 3
  ... = shard(computation, ...)

  The outputs from all shards are concatenated back together along their 0-th
  dimension.

  Inputs and outputs of the computation must be at least rank-1 Tensors.

  Args:
    computation: A Python function that builds a computation to apply to each
      shard of the input.
    inputs: A list of input tensors or None (equivalent to an empty
      list). The 0-th dimension of each Tensor must have size
      divisible by `num_shards`.
    num_shards: The number of shards.
    infeed_queue: If not `None`, the `InfeedQueue` from which to append a tuple
      of arguments as inputs to `computation`.
    device_assignment: If not `None`, a `DeviceAssignment` describing the
      mapping between logical cores in the computation with physical cores in
      the TPU topology. Uses a default device assignment if `None`. The
      `DeviceAssignment` may be omitted if each shard of the computation uses
      only one core, and there is either only one shard, or the number of shards
      is equal to the number of cores in the TPU system.
    name: (Deprecated) Does nothing.
  Returns:
    A list of output tensors.
  Raises:
    ValueError: If `num_shards <= 0`
  """
  return shard(
      computation,
      inputs,
      num_shards=num_shards,
      infeed_queue=infeed_queue,
      device_assignment=device_assignment,
      name=name)


def rewrite(computation,
            inputs=None,
            infeed_queue=None,
            device_assignment=None,
            name=None):
  """Rewrites `computation` for execution on a TPU system.

  Args:
    computation: A Python function that builds a computation to apply
      to the input. If the function takes n inputs, 'inputs' should be
      a list of n tensors. If the function returns m outputs, rewrite
      will return a list of m tensors.
    inputs: A list of input tensors or `None` (equivalent to an empty list).
    infeed_queue: If not `None`, the `InfeedQueue` from which to append a tuple
      of arguments as inputs to `computation`.
    device_assignment: if not `None`, a `DeviceAssignment` describing the
      mapping between logical cores in the computation with physical cores in
      the TPU topology. May be omitted for a single-core computation, in which
      case the core attached to task 0, TPU device 0 is used.
    name: (Deprecated) Does nothing.
  Returns:
    A list of output tensors.
  """
  if inputs is not None and not isinstance(inputs, (list, tuple)):
    raise TypeError("tpu.rewrite() inputs must be a list or tuple")

  # TODO(b/36647078) remove disable when pylint bug is fixed.
  # pylint: disable=indexing-exception
  return replicate(
      computation,
      None if inputs is None else [inputs],
      infeed_queue=infeed_queue,
      device_assignment=device_assignment,
      name=name)[0]
  # pylint: enable=indexing-exception
