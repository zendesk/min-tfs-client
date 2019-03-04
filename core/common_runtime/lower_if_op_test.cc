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

#include "tensorflow/core/common_runtime/lower_if_while.h"

#include "tensorflow/cc/client/client_session.h"
#include "tensorflow/cc/framework/ops.h"
#include "tensorflow/cc/ops/array_ops.h"
#include "tensorflow/cc/ops/control_flow_ops_internal.h"
#include "tensorflow/cc/ops/function_ops.h"
#include "tensorflow/cc/ops/resource_variable_ops.h"
#include "tensorflow/cc/ops/standard_ops.h"
#include "tensorflow/core/common_runtime/graph_runner.h"
#include "tensorflow/core/framework/function_testlib.h"
#include "tensorflow/core/framework/node_def_util.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/tensor_testutil.h"
#include "tensorflow/core/graph/graph_constructor.h"
#include "tensorflow/core/graph/graph_def_builder.h"
#include "tensorflow/core/graph/graph_def_builder_util.h"
#include "tensorflow/core/lib/core/status_test_util.h"
#include "tensorflow/core/lib/strings/str_util.h"
#include "tensorflow/core/platform/test.h"

namespace tensorflow {
namespace {

AttrValue FuncAttr(const string& name) {
  AttrValue attr;
  attr.mutable_func()->set_name(name);
  return attr;
}

Status Rewrite(std::unique_ptr<Graph>* graph) {
  FunctionLibraryDefinition flib_def((*graph)->flib_def());
  GraphOptimizationPassOptions opt_options;
  opt_options.graph = graph;
  opt_options.flib_def = &flib_def;
  LowerIfWhilePass pass;
  return pass.Run(opt_options);
}

TEST(LowerIfOpTest, Simple) {
  std::unique_ptr<Graph> graph(new Graph(OpRegistry::Global()));

  // Add test functions for then and else branch.
  FunctionDefLibrary f_lib_proto;
  *(f_lib_proto.add_function()) = test::function::XTimesTwo();
  *(f_lib_proto.add_function()) = test::function::XTimesFour();

  // Construct simple conditional that switches on `pred` and operates only on
  // single input `A`.
  Scope root = Scope::NewRootScope().ExitOnError();
  TF_ASSERT_OK(root.graph()->AddFunctionLibrary(f_lib_proto));
  auto a = ops::_Arg(root.WithOpName("A"), DT_INT32, 0);
  auto pred = ops::_Arg(root.WithOpName("pred"), DT_BOOL, 1);
  Node* written_if;
  std::vector<NodeBuilder::NodeOut> inputs({NodeBuilder::NodeOut(a.node())});
  TF_ASSERT_OK(NodeBuilder("if", "If", &root.graph()->flib_def())
                   .Input(pred.node())
                   .Input(inputs)
                   .Attr("then_branch", FuncAttr("XTimesTwo"))
                   .Attr("else_branch", FuncAttr("XTimesFour"))
                   .Attr(LowerIfWhilePass::kLowerUsingSwitchMergeAttr, true)
                   .Attr("Tout", {DT_INT32})
                   .Finalize(root.graph(), &written_if));
  TF_ASSERT_OK(root.DoShapeInference(written_if));
  TF_ASSERT_OK(root.ToGraph(graph.get()));

  // The input graph has no switch or merge nodes.
  int node_called_if_count = 0;
  for (const auto* op : graph->op_nodes()) {
    ASSERT_FALSE(op->IsSwitch());
    ASSERT_FALSE(op->IsMerge());
    if (op->name() == "if") {
      ++node_called_if_count;
    }
  }
  ASSERT_EQ(node_called_if_count, 1);

  TF_ASSERT_OK(Rewrite(&graph));

  // Verify the resultant graph has switch and merge nodes, and a node called
  // `if` (but not If nodes).
  int switch_count = 0;
  int merge_count = 0;
  node_called_if_count = 0;
  for (const auto* op : graph->op_nodes()) {
    if (op->IsSwitch()) {
      ++switch_count;
    }
    if (op->IsMerge()) {
      ++merge_count;
    }
    ASSERT_NE(op->type_string(), "If");
    if (op->name() == "if") {
      ++node_called_if_count;
    }
  }
  // One switch for predicate and one for input (A).
  ASSERT_EQ(switch_count, 2);
  // One merge for the single output value of then and else, and one more merge
  // to enforce then and else function call execution (`branch_executed` node).
  ASSERT_EQ(merge_count, 2);
  ASSERT_EQ(node_called_if_count, 1);

  // Verify execution.
  ClientSession session(root);
  {
    ClientSession::FeedType feeds;
    feeds.emplace(Output(pred.node()), Input::Initializer(false));
    feeds.emplace(Output(a.node()), Input::Initializer(10));
    std::vector<Tensor> out_tensors;
    TF_ASSERT_OK(session.Run(feeds, {Output(written_if)}, &out_tensors));
    EXPECT_EQ(out_tensors.size(), 1);
    EXPECT_EQ(out_tensors[0].scalar<int>()(), 40);
  }
  {
    ClientSession::FeedType feeds;
    feeds.emplace(Output(pred.node()), Input::Initializer(true));
    feeds.emplace(Output(a.node()), Input::Initializer(10));
    std::vector<Tensor> out_tensors;
    TF_ASSERT_OK(session.Run(feeds, {Output(written_if)}, &out_tensors));
    EXPECT_EQ(out_tensors.size(), 1);
    EXPECT_EQ(out_tensors[0].scalar<int>()(), 20);
  }
}

TEST(LowerIfOpTest, BranchFunctionsWithoutOutputs) {
  using ::tensorflow::test::function::GDef;
  using ::tensorflow::test::function::NDef;
  using FDH = ::tensorflow::FunctionDefHelper;

  // Wrap AssignAddVariable + Const into a function.
  const auto assign_add = [](const string& fn_name, int v) {
    const Tensor tensor = test::AsScalar<int32>(v);
    return FDH::Create(
        fn_name, {"v: resource"}, {}, {},
        {
            {{"c"}, "Const", {}, {{"value", tensor}, {"dtype", DT_INT32}}},
            {{"upd"},
             "AssignAddVariableOp",
             {"v", "c:output"},
             {{"dtype", DT_INT32}}},
        },
        /*ret_def=*/{},
        /*control_ret_def=*/{{"side_effects", "upd"}});
  };

  std::unique_ptr<Graph> graph(new Graph(OpRegistry::Global()));
  // Add test functions for then and else branch.
  FunctionDefLibrary f_lib_proto;
  *(f_lib_proto.add_function()) = assign_add("AddOne", 1);
  *(f_lib_proto.add_function()) = assign_add("AddTwo", 2);

  // Construct a graph to represent following program:
  //
  //  (pred: bool, initial_val: int32) -> (int32) {
  //    var = Variable(initial_value)
  //    if pred:
  //      var += 1    # AddOne function call
  //    else:
  //      var += 2    # AddTwo function call
  //    return var
  //  }
  Scope root = Scope::NewRootScope().ExitOnError();
  TF_ASSERT_OK(root.graph()->AddFunctionLibrary(f_lib_proto));

  auto pred = ops::_Arg(root.WithOpName("pred"), DT_BOOL, 0);
  auto initial_val = ops::_Arg(root.WithOpName("initial_val"), DT_INT32, 1);

  auto var = ops::VarHandleOp(root.WithOpName("var"), DT_INT32, {});
  auto init = ops::AssignVariableOp(root.WithOpName("init"), var, initial_val);

  Node* if_node;
  std::vector<NodeBuilder::NodeOut> inputs({NodeBuilder::NodeOut(var.node())});
  TF_ASSERT_OK(NodeBuilder("if", "If", &root.graph()->flib_def())
                   .Input(pred.node())
                   .Input(inputs)
                   .ControlInput(init.operation.node())
                   .Attr("then_branch", FuncAttr("AddOne"))
                   .Attr("else_branch", FuncAttr("AddTwo"))
                   .Attr(LowerIfWhilePass::kLowerUsingSwitchMergeAttr, true)
                   .Attr("Tout", DataTypeSlice{})
                   .Finalize(root.graph(), &if_node));

  auto read = ops::ReadVariableOp(
      root.WithOpName("read").WithControlDependencies(Output(if_node)), var,
      DT_INT32);

  TF_ASSERT_OK(root.DoShapeInference(if_node));
  TF_ASSERT_OK(root.ToGraph(graph.get()));

  TF_ASSERT_OK(Rewrite(&graph));

  // Verify the resultant graph has switch, merge and function call nodes.
  // TODO(ezhulenev): Inlining functions with empty outputs leads to undefined
  // graph execution.
  int switch_count = 0;
  int merge_count = 0;
  int node_called_if_count = 0;
  for (const auto* op : graph->op_nodes()) {
    if (op->IsSwitch()) ++switch_count;
    if (op->IsMerge()) ++merge_count;
    if (op->name() == "if") ++node_called_if_count;
    ASSERT_NE(op->type_string(), "If");
  }
  // One switch for predicate and one for input (A).
  ASSERT_EQ(switch_count, 2);
  // One merge for the else/then branch (`branch_executed` node).
  ASSERT_EQ(merge_count, 1);
  ASSERT_EQ(node_called_if_count, 1);

  // Verify execution.
  ClientSession session(root);
  {
    ClientSession::FeedType feeds;
    feeds.emplace(Output(pred.node()), Input::Initializer(true));
    feeds.emplace(Output(initial_val.node()), Input::Initializer(10));

    std::vector<Tensor> out_tensors;
    TF_ASSERT_OK(session.Run(feeds, {Output(read)}, &out_tensors));
    EXPECT_EQ(out_tensors.size(), 1);
    EXPECT_EQ(out_tensors[0].scalar<int>()(), 11);
  }
  {
    ClientSession::FeedType feeds;
    feeds.emplace(Output(pred.node()), Input::Initializer(false));
    feeds.emplace(Output(initial_val.node()), Input::Initializer(10));

    std::vector<Tensor> out_tensors;
    TF_ASSERT_OK(session.Run(feeds, {Output(read)}, &out_tensors));
    EXPECT_EQ(out_tensors.size(), 1);
    EXPECT_EQ(out_tensors[0].scalar<int>()(), 12);
  }
}

}  // namespace
}  // namespace tensorflow
