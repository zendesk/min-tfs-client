"""Definitions for rules to fuzz TensorFlow."""

# TensorFlow fuzzing can be done in open source too, as it is in oss-fuzz.com

# tf_cc_fuzz_test is a cc_test modified to include fuzzing support and dependencies for go/fuzztest.
def tf_cc_fuzz_test(
        name,
        # Bug reporting component (unused in oss)
        componentid = None,
        # Additional cc_test control
        data = [],
        deps = [],
        tags = [],
        **kwargs):
    """Specify how to build a TensorFlow fuzz target.

    Args:
      name: Mandatory name of the fuzzer target.

      componentid: The buganizer component ID for filing bugs
        found by this fuzz test. By default, we use TensorFlow Security
        component.

      data: Additional data dependencies passed to the underlying cc_test rule.

      deps: An optional list of dependencies for the code you're fuzzing.

      tags: Additional tags passed to the underlying cc_test rule.

      **kwargs: Collects all remaining arguments and passes them to the
        underlying cc_test rule generated by the macro.
    """
    componentid = None

    # Fuzzers in open source must be run manually
    tags = tags + ["manual"]

    # Now, redirect to cc_test
    native.cc_test(
        name = name,
        deps = deps + [
            "@com_google_fuzztest//fuzztest",
            "@com_google_fuzztest//fuzztest:fuzztest_gtest_main",
        ],
        data = data,
        tags = tags,
        linkstatic = 1,
        **kwargs
    )

# tf_py_fuzz_target is a py_test modified to include fuzzing support.
def tf_py_fuzz_target(
        name,
        # Fuzzing specific arguments
        fuzzing_dict = [],
        corpus = [],
        parsers = [],
        # Reporting bugs arguments, not used in open source
        componentid = None,
        hotlists = [],
        # Additional py_test control
        python_version = None,
        data = [],
        deps = [],
        tags = [],
        # Remaining py_test arguments
        **kwargs):
    """Specify how to build a TensorFlow Python fuzz target.

    Args:
      name: Mandatory name of the fuzzer target.

      fuzzing_dict: An optional a set of dictionary files following
        the AFL/libFuzzer dictionary syntax.

      corpus: An optional set of files used as the initial test corpus
        for the target. When doing "bazel test" in the default null-fuzzer
        (unittest) mode, these files are automatically passed to the target
        function.

      parsers: An optional list of file extensions that the target supports.
        Used by tools like autofuzz to reuse corpus sets across targets.

      componentid: Used internally for reporting fuzz discovered bugs.

      hotlists: Used internally for reporting fuzz discovered bugs.

      python_version: Python version to run target with.

      data: Additional data dependencies passed to the underlying py_test rule.

      deps: An optional list of dependencies for the code you're fuzzing.

      tags: Additional tags passed to the underlying py_test rule.

      **kwargs: Collects all remaining arguments and passes them to the
        underlying py_test rule generated by the macro.
    """
    componentid = None
    hotlists = None
    python_version = python_version or "PY3"

    # Fuzzers in open source must be run manually
    tags = tags + ["manual"]

    # Now, redirect to py_test
    native.py_test(
        name = name,
        python_version = python_version,
        deps = deps,
        data = data,
        tags = tags,
        **kwargs
    )
