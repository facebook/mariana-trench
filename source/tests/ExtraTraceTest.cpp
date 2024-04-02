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

TEST_F(ExtraTraceTest, CallInfoSerializationDeserialization) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  {
    const auto* kind = context.kind_factory->get("TestKind");
    const auto* callee = context.methods->create(
        redex::create_void_method(scope, "LClass;", "one"));
    const auto* call_position = context.positions->get(std::nullopt, 1);
    const auto* callee_port = context.access_path_factory->get(
        AccessPath(Root(Root::Kind::Argument, 0)));
    auto extra_trace = ExtraTrace(
        kind,
        callee,
        call_position,
        callee_port,
        CallKind::propagation_with_trace(CallKind::callsite().encode()));
    EXPECT_EQ(
        ExtraTrace::from_json(extra_trace.to_json(), context), extra_trace);
  }
}

} // namespace marianatrench
