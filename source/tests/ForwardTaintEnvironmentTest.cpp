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

TEST_F(ForwardTaintEnvironmentTest, LessOrEqual) {
  EXPECT_TRUE(MemoryLocationEnvironment().leq(MemoryLocationEnvironment()));
  EXPECT_TRUE(TaintEnvironment().leq(TaintEnvironment()));
  EXPECT_TRUE(ForwardTaintEnvironment().leq(ForwardTaintEnvironment()));
}

TEST_F(ForwardTaintEnvironmentTest, LessOrEqualSuperSet) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kind_factory->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      redex::create_void_method(scope, "LClass;", "method"));
  auto* return_port =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Return)));
  auto* method_origin =
      context.origin_factory->method_origin(method, return_port);

  auto memory_location = std::make_unique<ParameterMemoryLocation>(0);
  auto domain1 = TaintEnvironment{
      {memory_location.get(),
       TaintTree{Taint{test::make_leaf_taint_config(source_kind)}}}};
  EXPECT_TRUE(TaintEnvironment{}.leq(domain1));
  EXPECT_FALSE(domain1.leq(TaintEnvironment{}));

  auto domain2 = TaintEnvironment{
      {memory_location.get(),
       TaintTree{Taint{
           test::make_leaf_taint_config(source_kind),
           test::make_taint_config(
               source_kind,
               test::FrameProperties{
                   .callee_port = context.access_path_factory->get(
                       AccessPath(Root(Root::Kind::Return))),
                   .callee = method,
                   .distance = 1,
                   .origins = OriginSet{method_origin},
                   .call_kind = CallKind::callsite()})}}}};

  EXPECT_TRUE(domain1.leq(domain2));
  EXPECT_FALSE(domain2.leq(domain1));
}

TEST_F(ForwardTaintEnvironmentTest, LessOrEqualDifferentSources) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kind_factory->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      redex::create_void_method(scope, "LClass;", "method"));
  auto* return_port =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Return)));
  auto* method_origin =
      context.origin_factory->method_origin(method, return_port);
  auto memory_location = std::make_unique<ParameterMemoryLocation>(0);

  auto domain1 = TaintEnvironment{
      {memory_location.get(),
       TaintTree{Taint{test::make_leaf_taint_config(source_kind)}}}};

  auto domain2 = TaintEnvironment{
      {memory_location.get(),
       TaintTree{Taint{test::make_taint_config(
           source_kind,
           test::FrameProperties{
               .callee_port = context.access_path_factory->get(
                   AccessPath(Root(Root::Kind::Return))),
               .callee = method,
               .call_position = context.positions->unknown(),
               .distance = 1,
               .origins = OriginSet{method_origin},
               .call_kind = CallKind::callsite()})}}}};

  EXPECT_FALSE(domain1.leq(domain2));
  EXPECT_FALSE(domain2.leq(domain1));
}

TEST_F(ForwardTaintEnvironmentTest, JoinSuperSet) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kind_factory->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      redex::create_void_method(scope, "LClass;", "method"));
  auto* return_port =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Return)));
  auto* method_origin =
      context.origin_factory->method_origin(method, return_port);
  auto memory_location = std::make_unique<ParameterMemoryLocation>(0);

  auto domain1 = TaintEnvironment{
      {memory_location.get(),
       TaintTree{Taint{test::make_leaf_taint_config(source_kind)}}}};
  auto domain2 = TaintEnvironment{
      {memory_location.get(),
       TaintTree{Taint{
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
                   .call_kind = CallKind::callsite()})}}}};
  domain1.join_with(domain2);
  EXPECT_TRUE(domain1 == domain2);
}

TEST_F(ForwardTaintEnvironmentTest, JoinTwoDifferent) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kind_factory->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      redex::create_void_method(scope, "LClass;", "method"));
  auto* return_port =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Return)));
  auto* method_origin =
      context.origin_factory->method_origin(method, return_port);
  auto memory_location = std::make_unique<ParameterMemoryLocation>(0);

  auto domain1 = TaintEnvironment{
      {memory_location.get(),
       TaintTree{Taint{test::make_leaf_taint_config(source_kind)}}}};

  auto domain2 = TaintEnvironment{
      {memory_location.get(),
       TaintTree{Taint{test::make_taint_config(
           source_kind,
           test::FrameProperties{
               .callee_port = context.access_path_factory->get(
                   AccessPath(Root(Root::Kind::Return))),
               .callee = method,
               .call_position = context.positions->unknown(),
               .distance = 1,
               .origins = OriginSet{method_origin},
               .call_kind = CallKind::callsite()})}}}};

  auto domain3 = TaintEnvironment{
      {memory_location.get(),
       TaintTree{Taint{
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
                   .call_kind = CallKind::callsite()})}}}};
  domain1.join_with(domain2);
  EXPECT_TRUE(domain1 == domain3);
}

TEST_F(ForwardTaintEnvironmentTest, JoinTwoEnvironmentWithDifferentSources) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kind_factory->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      redex::create_void_method(scope, "LClass;", "method"));
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
