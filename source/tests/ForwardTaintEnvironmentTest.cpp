/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/ForwardTaintEnvironment.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

#include <memory>

namespace marianatrench {

class ForwardTaintEnvironmentTest : public test::Test {};

TEST_F(ForwardTaintEnvironmentTest, JoinTwoEnvironmentWithDifferentSources) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kind_factory->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      marianatrench::redex::create_void_method(scope, "LClass;", "method"));
  auto* return_port =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Return)));
  auto* method_origin =
      context.origin_factory->method_origin(method, return_port);

  auto parameter_1 = std::make_unique<ParameterMemoryLocation>(1);
  auto parameter_2 = std::make_unique<ParameterMemoryLocation>(2);

  auto environment1 = ForwardTaintEnvironment::initial();
  environment1.write(parameter_1.get(), {}, UpdateKind::Weak);
  environment1.write(
      parameter_2.get(),
      TaintTree{Taint{test::make_leaf_taint_config(source_kind)}},
      UpdateKind::Weak);

  auto environment2 = ForwardTaintEnvironment::initial();
  environment2.write(
      parameter_1.get(),
      TaintTree{Taint{test::make_leaf_taint_config(source_kind)}},
      UpdateKind::Weak);
  environment2.write(
      parameter_2.get(),
      TaintTree{Taint{test::make_taint_config(
          source_kind,
          test::FrameProperties{
              .callee_port = context.access_path_factory->get(
                  AccessPath(Root(Root::Kind::Return))),
              .callee = method,
              .call_position = context.positions->unknown(),
              .distance = 1,
              .origins = OriginSet{method_origin},
              .call_kind = CallKind::callsite()})}},
      UpdateKind::Weak);

  environment1.join_with(environment2);

  EXPECT_EQ(
      environment1.read(parameter_1.get()),
      TaintTree{Taint{test::make_leaf_taint_config(source_kind)}});
  EXPECT_EQ(
      environment1.read(parameter_2.get()),
      (TaintTree{Taint{
          test::make_leaf_taint_config(source_kind),
          test::make_taint_config(
              source_kind,
              test::FrameProperties{
                  .callee_port = context.access_path_factory->get(
                      AccessPath(Root(Root::Kind::Return))),
                  .callee = method,
                  .call_position = context.positions->unknown(),
                  .distance = 1,
                  .origins = OriginSet{method_origin},
                  .call_kind = CallKind::callsite()})}}));
}

} // namespace marianatrench
