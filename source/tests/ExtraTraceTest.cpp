/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>
#include <gmock/gmock.h>

#include <mariana-trench/ExtraTrace.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class ExtraTraceTest : public test::Test {};

TEST_F(ExtraTraceTest, ExtraTraceConstruct) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  const auto* kind = context.kind_factory->get("TestKind");
  const auto* callee = context.methods->create(redex::create_void_method(
      scope,
      /* class_name */ "LClass;",
      /* method_name */ "one",
      /* parameter_types */ "LClass;",
      /* return_type */ "LClass;"));
  const auto* call_position = context.positions->get(std::nullopt, 1);
  const auto* callee_port_arg0 = context.access_path_factory->get(
      AccessPath(Root(Root::Kind::Argument, 0)));
  const auto* callee_port_return =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Return)));

  // Test extra-trace for propagation with trace
  EXPECT_NO_THROW(ExtraTrace(
      kind,
      callee,
      call_position,
      callee_port_arg0,
      CallKind::propagation_with_trace(CallKind::callsite().encode()),
      FrameType::sink()));

  EXPECT_THROW(
      ExtraTrace(
          kind,
          callee,
          call_position,
          callee_port_arg0,
          CallKind::propagation_with_trace(CallKind::callsite().encode()),
          FrameType::source()),
      RedexException);

  // Test source or sink trace as extra-trace
  EXPECT_NO_THROW(ExtraTrace(
      kind,
      callee,
      call_position,
      callee_port_return,
      CallKind::callsite(),
      FrameType::source()));

  EXPECT_NO_THROW(ExtraTrace(
      kind,
      /* callee */ nullptr,
      call_position,
      callee_port_return,
      CallKind::origin(),
      FrameType::sink()));

  // extra-trace with call-kind origin should not have a callee.
  EXPECT_THROW(
      ExtraTrace(
          kind,
          callee,
          call_position,
          callee_port_return,
          CallKind::origin(),
          FrameType::sink()),
      boost::exception_detail::error_info_injector<RedexException>);
}

TEST_F(ExtraTraceTest, CallInfoSerializationDeserialization) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  const auto* kind = context.kind_factory->get("TestKind");
  const auto* callee = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  const auto* call_position = context.positions->get(std::nullopt, 1);
  const auto* callee_port = context.access_path_factory->get(
      AccessPath(Root(Root::Kind::Argument, 0)));

  // propagation-with-trace with call-info callsite
  {
    auto extra_trace = ExtraTrace(
        kind,
        callee,
        call_position,
        callee_port,
        CallKind::propagation_with_trace(CallKind::callsite().encode()),
        FrameType::sink());
    EXPECT_EQ(
        ExtraTrace::from_json(extra_trace.to_json(), context), extra_trace);
  }

  // source trace with call-info callsite
  {
    auto extra_trace = ExtraTrace(
        kind,
        callee,
        call_position,
        callee_port,
        CallKind::callsite(),
        FrameType::source());
    EXPECT_EQ(
        ExtraTrace::from_json(extra_trace.to_json(), context), extra_trace);
  }

  // sink trace with call-info origin
  {
    auto extra_trace = ExtraTrace(
        kind,
        /* callee */ nullptr,
        call_position,
        callee_port,
        CallKind::origin(),
        FrameType::source());
    EXPECT_EQ(
        ExtraTrace::from_json(extra_trace.to_json(), context), extra_trace);
  }
}

} // namespace marianatrench
