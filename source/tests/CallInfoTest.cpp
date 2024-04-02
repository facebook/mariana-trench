/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>
#include <gmock/gmock.h>

#include <mariana-trench/CallInfo.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class CallInfoTest : public test::Test {};

TEST_F(CallInfoTest, CallInfoSerializationDeserialization) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  {
    auto call_info = CallInfo::make_default();
    EXPECT_EQ(CallInfo::from_json(call_info.to_json(), context), call_info);
  }

  {
    const auto* callee = context.methods->create(
        redex::create_void_method(scope, "LClass;", "one"));
    const auto* callee_port = context.access_path_factory->get(
        AccessPath(Root(Root::Kind::Argument, 0)));
    const auto* call_position = context.positions->get(std::nullopt, 1);
    auto call_info =
        CallInfo(callee, CallKind::callsite(), callee_port, call_position);
    EXPECT_EQ(CallInfo::from_json(call_info.to_json(), context), call_info);
  }
}

TEST_F(CallInfoTest, CallKindSerializationDeserialization) {
  {
    auto call_kind = CallKind::declaration();
    EXPECT_EQ(
        CallKind::from_trace_string(call_kind.to_trace_string()), call_kind);

    auto call_kind_with_trace =
        CallKind::propagation_with_trace(call_kind.encode());
    EXPECT_EQ(
        CallKind::from_trace_string(call_kind_with_trace.to_trace_string()),
        call_kind_with_trace);
  }

  {
    auto call_kind = CallKind::origin();
    EXPECT_EQ(
        CallKind::from_trace_string(call_kind.to_trace_string()), call_kind);

    auto call_kind_with_trace =
        CallKind::propagation_with_trace(call_kind.encode());
    EXPECT_EQ(
        CallKind::from_trace_string(call_kind_with_trace.to_trace_string()),
        call_kind_with_trace);
  }

  {
    auto call_kind = CallKind::callsite();
    EXPECT_EQ(
        CallKind::from_trace_string(call_kind.to_trace_string()), call_kind);

    auto call_kind_with_trace =
        CallKind::propagation_with_trace(call_kind.encode());
    EXPECT_EQ(
        CallKind::from_trace_string(call_kind_with_trace.to_trace_string()),
        call_kind_with_trace);
  }

  {
    auto call_kind = CallKind::propagation();
    EXPECT_EQ(
        CallKind::from_trace_string(call_kind.to_trace_string()), call_kind);
  }

  EXPECT_THROW(
      CallKind::from_trace_string("PropagationWithTrace:YOLODeclaration"),
      JsonValidationError);
  EXPECT_THROW(
      CallKind::from_trace_string("YOLODeclaration"), JsonValidationError);
  EXPECT_THROW(
      CallKind::from_trace_string("TotallyInvalid"), JsonValidationError);
}

} // namespace marianatrench
