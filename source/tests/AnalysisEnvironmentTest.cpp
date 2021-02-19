/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/AnalysisEnvironment.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

#include <memory>

namespace marianatrench {

class EnvironmentTest : public test::Test {};

TEST_F(EnvironmentTest, LessOrEqual) {
  EXPECT_TRUE(MemoryLocationsPartition().leq(MemoryLocationsPartition()));
  EXPECT_TRUE(TaintAbstractPartition().leq(TaintAbstractPartition()));
  EXPECT_TRUE(AnalysisEnvironment().leq(AnalysisEnvironment()));
}

TEST_F(EnvironmentTest, LessOrEqualSuperSet) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kinds->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      redex::create_void_method(scope, "LClass;", "method"));

  auto domain1 = TaintAbstractPartition{
      {nullptr, TaintTree{Taint{Frame::leaf(source_kind)}}}};
  EXPECT_TRUE(TaintAbstractPartition{}.leq(domain1));
  EXPECT_FALSE(domain1.leq(TaintAbstractPartition{}));

  auto domain2 = TaintAbstractPartition{
      {nullptr,
       TaintTree{Taint{
           Frame::leaf(source_kind),
           Frame(
               source_kind,
               /* callee_port */ AccessPath(Root(Root::Kind::Return)),
               /* callee */ method,
               /* call_position */ context.positions->unknown(),
               /* distance */ 1,
               /* origins */ MethodSet{method},
               /* inferred_features */ {},
               /* user_features */ {},
               /* via_type_of_ports */ {},
               /* local_positions */ {}),
       }}}};

  EXPECT_TRUE(domain1.leq(domain2));
  EXPECT_FALSE(domain2.leq(domain1));
}

TEST_F(EnvironmentTest, LessOrEqualDifferentSources) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kinds->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      redex::create_void_method(scope, "LClass;", "method"));

  auto domain1 = TaintAbstractPartition{
      {nullptr, TaintTree{Taint{Frame::leaf(source_kind)}}}};

  auto domain2 = TaintAbstractPartition{
      {nullptr,
       TaintTree{Taint{
           Frame(
               source_kind,
               /* callee_port */ AccessPath(Root(Root::Kind::Return)),
               /* callee */ method,
               /* call_position */ context.positions->unknown(),
               /* distance */ 1,
               /* origins */ MethodSet{method},
               /* inferred_features */ {},
               /* user_features */ {},
               /* via_type_of_ports */ {},
               /* local_positions */ {}),
       }}}};

  EXPECT_FALSE(domain1.leq(domain2));
  EXPECT_FALSE(domain2.leq(domain1));
}

TEST_F(EnvironmentTest, JoinSuperSet) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kinds->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      redex::create_void_method(scope, "LClass;", "method"));

  auto domain1 = TaintAbstractPartition{
      {nullptr, TaintTree{Taint{Frame::leaf(source_kind)}}}};
  auto domain2 = TaintAbstractPartition{
      {nullptr,
       TaintTree{Taint{
           Frame::leaf(source_kind),
           Frame(
               source_kind,
               /* callee_port */ AccessPath(Root(Root::Kind::Return)),
               /* callee */ method,
               /* call_position */ context.positions->unknown(),
               /* distance */ 1,
               /* origins */ MethodSet{method},
               /* inferred_features */ {},
               /* user_features */ {},
               /* via_type_of_ports */ {},
               /* local_positions */ {}),
       }}}};
  domain1.join_with(domain2);
  EXPECT_TRUE(domain1 == domain2);
}

TEST_F(EnvironmentTest, JoinTwoDifferent) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kinds->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      redex::create_void_method(scope, "LClass;", "method"));

  auto domain1 = TaintAbstractPartition{
      {nullptr, TaintTree{Taint{Frame::leaf(source_kind)}}}};

  auto domain2 = TaintAbstractPartition{
      {nullptr,
       TaintTree{Taint{
           Frame(
               source_kind,
               /* callee_port */ AccessPath(Root(Root::Kind::Return)),
               /* callee */ method,
               /* call_position */ context.positions->unknown(),
               /* distance */ 1,
               /* origins */ MethodSet{method},
               /* inferred_features */ {},
               /* user_features */ {},
               /* via_type_of_ports */ {},
               /* local_positions */ {}),
       }}}};

  auto domain3 = TaintAbstractPartition{
      {nullptr,
       TaintTree{Taint{
           Frame::leaf(source_kind),
           Frame(
               source_kind,
               /* callee_port */ AccessPath(Root(Root::Kind::Return)),
               /* callee */ method,
               /* call_position */ context.positions->unknown(),
               /* distance */ 1,
               /* origins */ MethodSet{method},
               /* inferred_features */ {},
               /* user_features */ {},
               /* via_type_of_ports */ {},
               /* local_positions */ {}),
       }}}};
  domain1.join_with(domain2);
  EXPECT_TRUE(domain1 == domain3);
}

TEST_F(EnvironmentTest, JoinTwoEnvironmentWithDifferentSources) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kinds->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      redex::create_void_method(scope, "LClass;", "method"));

  auto parameter_1 = std::make_unique<ParameterMemoryLocation>(1);
  auto parameter_2 = std::make_unique<ParameterMemoryLocation>(2);

  auto environment1 = AnalysisEnvironment::initial();
  environment1.write(parameter_1.get(), {}, UpdateKind::Weak);
  environment1.write(
      parameter_2.get(),
      TaintTree{Taint{Frame::leaf(source_kind)}},
      UpdateKind::Weak);

  auto environment2 = AnalysisEnvironment::initial();
  environment2.write(
      parameter_1.get(),
      TaintTree{Taint{Frame::leaf(source_kind)}},
      UpdateKind::Weak);
  environment2.write(
      parameter_2.get(),
      TaintTree{Taint{
          Frame(
              source_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ method,
              /* call_position */ context.positions->unknown(),
              /* distance */ 1,
              /* origins */ MethodSet{method},
              /* inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {}),
      }},
      UpdateKind::Weak);

  environment1.join_with(environment2);

  EXPECT_EQ(
      environment1.read(parameter_1.get()),
      TaintTree{Taint{Frame::leaf(source_kind)}});
  EXPECT_EQ(
      environment1.read(parameter_2.get()),
      (TaintTree{Taint{
          Frame::leaf(source_kind),
          Frame(
              source_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ method,
              /* call_position */ context.positions->unknown(),
              /* distance */ 1,
              /* origins */ MethodSet{method},
              /* inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {}),
      }}));
}

} // namespace marianatrench
