/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/Redex.h>
#include <mariana-trench/TaintEnvironment.h>
#include <mariana-trench/tests/Test.h>

#include <memory>

namespace marianatrench {

class TaintEnvironmentTest : public test::Test {};

TEST_F(TaintEnvironmentTest, LessOrEqualSuperSet) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kind_factory->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      marianatrench::redex::create_void_method(scope, "LClass;", "method"));
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

TEST_F(TaintEnvironmentTest, LessOrEqualDifferentSources) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kind_factory->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      marianatrench::redex::create_void_method(scope, "LClass;", "method"));
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

TEST_F(TaintEnvironmentTest, JoinSuperSet) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kind_factory->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      marianatrench::redex::create_void_method(scope, "LClass;", "method"));
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

TEST_F(TaintEnvironmentTest, JoinTwoDifferent) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kind_factory->get("TestSource");

  Scope scope;
  auto* method = context.methods->create(
      marianatrench::redex::create_void_method(scope, "LClass;", "method"));
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

TEST_F(TaintEnvironmentTest, DeepWriteAndRead) {
  auto context = test::make_empty_context();

  // Setup memory locations
  auto m1 = std::make_unique<ParameterMemoryLocation>(1);
  auto m2 = std::make_unique<ParameterMemoryLocation>(2);
  auto m3 = std::make_unique<ParameterMemoryLocation>(3);
  auto m4 = std::make_unique<ParameterMemoryLocation>(4);
  auto m5 = std::make_unique<ParameterMemoryLocation>(5);
  auto m6 = std::make_unique<ParameterMemoryLocation>(6);
  auto m7 = std::make_unique<ParameterMemoryLocation>(7);
  auto m8 = std::make_unique<ParameterMemoryLocation>(8);

  // Setup fields
  const auto left = PathElement::field("left");
  const auto right = PathElement::field("right");
  const auto thisn1 = PathElement::field("this$1");

  auto alias_widen_broaden_feature = FeatureMayAlwaysSet{
      context.feature_factory->get_alias_broadening_feature()};

  // Setup field memory locations
  auto* m1_left = m1->make_field(left.name());
  auto* m1_right = m1->make_field(right.name());
  auto* m1_thisn1 = m1->make_field(thisn1.name());

  auto points_to_environment = PointsToEnvironment{
      // root
      {m1.get(),
       PointsToTree{
           // normal branch
           {Path{left}, PointsToSet{m2.get(), AliasingProperties::empty()}},
           // widened branch. Expect collapse-depth to be set to 0 when widened.
           {Path{right}, PointsToSet{m3.get(), AliasingProperties::empty()}},
           // branch with collapse-depth 1
           {Path{thisn1},
            PointsToSet{
                m4.get(),
                AliasingProperties::with_collapse_depth(CollapseDepth{1})}},
       }},
      {m2.get(),
       PointsToTree{
           {Path{left}, PointsToSet{m5.get()}},
       }},
      {m3.get(),
       PointsToTree{
           {Path{left}, PointsToSet{m6.get()}},
       }},
      {m6.get(),
       PointsToTree{
           // introduce a cycle back to m3.
           {Path{right}, PointsToSet{m3.get()}},
           // this path is independent of the cycle.
           {Path{left}, PointsToSet{m7.get()}},
       }},
      {m4.get(),
       PointsToTree{
           {Path{left}, PointsToSet{m8.get()}},
       }},
  };
  // Build the widening resolver
  auto widening_resolver = points_to_environment.make_widening_resolver();

  // Empty taint environment
  TaintEnvironment environment;

  // -------------------------------
  // Tests for normal normal branch
  // -------------------------------
  // m5 is normal branch leaf memory location
  const auto* m5_source = context.kind_factory->get("M5");
  auto m5_taint_tree =
      TaintTree{Taint{test::make_leaf_taint_config(m5_source)}};

  environment.deep_write(
      widening_resolver, m5.get(), Path{}, m5_taint_tree, UpdateKind::Strong);

  EXPECT_EQ(environment.read(m5.get()), m5_taint_tree);

  // m2 is normal branch with m5 at path .left on the points-to-tree. Any write
  // to .left path from m2 is written to m5.
  const auto* m2_source = context.kind_factory->get("M2");
  auto m2_taint = TaintTree{Taint{test::make_leaf_taint_config(m2_source)}};
  environment.deep_write(
      widening_resolver, m2.get(), Path{left}, m2_taint, UpdateKind::Weak);
  auto m2_m5_taint_tree = m2_taint.join(m5_taint_tree);
  EXPECT_EQ(environment.read(m5.get()), m2_m5_taint_tree);

  // Write to any other path from m2 will be
  // flattened and written here.
  const auto* m2_right_source = context.kind_factory->get("M2-right");
  auto m2_right_taint = Taint{test::make_leaf_taint_config(m2_right_source)};
  environment.deep_write(
      widening_resolver,
      m2.get(),
      Path{right},
      TaintTree{m2_right_taint},
      UpdateKind::Weak);
  EXPECT_EQ(
      environment.read(m2.get()), (TaintTree{{Path{right}, m2_right_taint}}));

  // Deep read from m2 will have the taint from m5 combined.
  auto m2_deep_taint_tree = TaintTree{
      {Path{right}, m2_right_taint},
  };
  m2_deep_taint_tree.write(Path{left}, m2_m5_taint_tree, UpdateKind::Weak);
  EXPECT_EQ(
      environment.deep_read(widening_resolver, m2.get()), m2_deep_taint_tree);

  // Same for deep Read from m1
  EXPECT_EQ(
      environment.deep_read(widening_resolver, m1_left), m2_deep_taint_tree);
  EXPECT_EQ(
      environment.deep_read(
          widening_resolver, MemoryLocationsDomain{m1.get()}, Path{left}),
      m2_deep_taint_tree);

  // -------------------------------
  // Tests for widened branch
  // -------------------------------
  // m3 is the head of the widened component. Taint is written here.
  const auto* m3_source = context.kind_factory->get("M3");
  auto m3_taint = Taint{test::make_leaf_taint_config(m3_source)};

  environment.deep_write(
      widening_resolver,
      m3.get(),
      Path{},
      TaintTree{m3_taint},
      UpdateKind::Strong);
  EXPECT_EQ(environment.read(m3.get()), (TaintTree{m3_taint}));

  // m6 is part of a widened component with m3 as it's head. Taint is never
  // written here but to m3 instead.
  const auto* m6_source = context.kind_factory->get("M6");
  auto m6_taint = Taint{test::make_leaf_taint_config(m6_source)};
  environment.deep_write(
      widening_resolver,
      m6.get(),
      Path{},
      TaintTree{m6_taint},
      UpdateKind::Weak);
  EXPECT_TRUE(environment.read(m6.get()).is_bottom());
  auto m3_m6_taint = m3_taint.join(m6_taint);
  EXPECT_EQ(environment.read(m3.get()), (TaintTree{m3_m6_taint}));

  // m6 has a .right path on the points-to-tree but that gets collapsed. Hence
  // a write to the path .right through m6 will be flattened and written here,
  // same as any other non-existent path in the points-to-tree.
  const auto* m6_right_source = context.kind_factory->get("M6-right");
  auto m6_right_taint = Taint{test::make_leaf_taint_config(m6_right_source)};
  environment.deep_write(
      widening_resolver,
      m6.get(),
      Path{right},
      TaintTree{m6_right_taint},
      UpdateKind::Weak);
  EXPECT_TRUE(environment.read(m6.get()).is_bottom());
  auto m3_taint_tree = TaintTree{
      {Path{}, m3_m6_taint},
      {Path{right}, m6_right_taint},
  };
  EXPECT_EQ(environment.read(m3.get()), m3_taint_tree);

  // Since m3 is widened, it's collapse-depth is set to always collapse hence
  // the taint tree is collapsed to a single node and via-alias-widen-broaden
  // feature is added.
  auto m3_deep_taint_tree = m3_taint_tree;
  m3_deep_taint_tree.collapse_deeper_than(0, alias_widen_broaden_feature);
  EXPECT_EQ(
      environment.deep_read(widening_resolver, m3.get()), m3_deep_taint_tree);
  // Same for deep read from m1
  EXPECT_EQ(
      environment.deep_read(widening_resolver, m1_right), m3_deep_taint_tree);
  EXPECT_EQ(
      environment.deep_read(
          widening_resolver, MemoryLocationsDomain{m1.get()}, Path{right}),
      m3_deep_taint_tree);

  // m6 has a .left path on the points-to-tree that is independent of the
  // widened component and hence is maintained. Any write to the .left path
  // through m6 will be written to m7 directly.
  const auto* m7_source = context.kind_factory->get("M7");
  auto m7_taint = Taint{test::make_leaf_taint_config(m7_source)};
  environment.deep_write(
      widening_resolver,
      m6.get(),
      Path{left},
      TaintTree{m7_taint},
      UpdateKind::Weak);
  // Check written at m7 directly. i.e. accurate aliasing is maintained without
  // widening for the independent side branch.
  EXPECT_EQ(environment.read(m7.get()), TaintTree{m7_taint});
  EXPECT_TRUE(environment.read(m6.get()).is_bottom());
  // Read at m3 is still the same as before.
  EXPECT_EQ(environment.read(m3.get()), m3_taint_tree);

  // Deep read sees the taint from m7 but is collapsed since m3 is widened.
  m3_taint_tree.write(Path{left}, m7_taint, UpdateKind::Weak);
  auto m3_collapsed_taint_tree =
      TaintTree{m3_taint_tree.collapse(alias_widen_broaden_feature)};
  EXPECT_EQ(
      environment.deep_read(widening_resolver, m3.get()),
      m3_collapsed_taint_tree);
  // Same for deep read from m1
  EXPECT_EQ(
      environment.deep_read(widening_resolver, m1_right),
      m3_collapsed_taint_tree);
  EXPECT_EQ(
      environment.deep_read(
          widening_resolver, MemoryLocationsDomain{m1.get()}, Path{right}),
      m3_collapsed_taint_tree);

  // ---------------------------------------
  // Test for branch with collapse-depth 1
  // ---------------------------------------
  // m4 is a branch which is added to .this$1 of m1 with collapse-depth set to 1
  // via the AliasingProperties. But m4 itself can hold taint that is more
  // precise.
  const auto* m8_source = context.kind_factory->get("M8");
  auto m8_taint = Taint{test::make_leaf_taint_config(m8_source)};
  const auto foo = PathElement::field("foo");
  const auto bar = PathElement::field("bar");
  const auto baz = PathElement::field("baz");
  const auto* baz_source = context.kind_factory->get("baz");
  auto baz_taint = Taint{test::make_leaf_taint_config(baz_source)};

  // m4 itself has a path .left with m8 on the points-to-tree.
  // Taint in .left.foo should be written to m8 directly at path .foo.
  environment.deep_write(
      widening_resolver,
      m4.get(),
      Path{left, right},
      TaintTree{m8_taint},
      UpdateKind::Weak);
  EXPECT_EQ(environment.read(m8.get()), (TaintTree{{Path{right}, m8_taint}}));
  EXPECT_TRUE(environment.read(m4.get()).is_bottom());
  auto m4_deep_taint_tree = TaintTree{{Path{left, right}, m8_taint}};
  EXPECT_EQ(
      environment.deep_read(widening_resolver, m4.get()), m4_deep_taint_tree);

  // Taint in .foo.bar.baz should be written to m4.
  environment.deep_write(
      widening_resolver,
      m4.get(),
      Path{foo, bar, baz},
      TaintTree{baz_taint},
      UpdateKind::Weak);
  auto m4_taint_tree = TaintTree{{Path{foo, bar, baz}, baz_taint}};
  EXPECT_EQ(environment.read(m4.get()), m4_taint_tree);

  // Deep read sees taint from m4 and m8
  m4_deep_taint_tree.join_with(m4_taint_tree);
  EXPECT_EQ(
      environment.deep_read(widening_resolver, m4.get()), m4_deep_taint_tree);

  // When reading the taint from m1 at path .this$1, the AliasingProperties
  // for the taint at memory location m4 has collapse-depth 1 and should be
  // collapsed to that depth.
  m4_deep_taint_tree.collapse_deeper_than(1, alias_widen_broaden_feature);
  EXPECT_EQ(
      environment.deep_read(widening_resolver, m1_thisn1), m4_deep_taint_tree);
  EXPECT_EQ(
      environment.deep_read(
          widening_resolver, MemoryLocationsDomain{m1.get()}, Path{thisn1}),
      m4_deep_taint_tree);

  // Full taint tree for m1
  const auto* m1_source = context.kind_factory->get("M1");
  auto m1_taint = Taint{test::make_leaf_taint_config(m1_source)};
  environment.deep_write(
      widening_resolver,
      m1.get(),
      Path{},
      TaintTree{m1_taint},
      UpdateKind::Weak);

  TaintTree m1_deep_taint_tree{
      {Path{}, m1_taint},
  };
  m1_deep_taint_tree.write(Path{left}, m2_deep_taint_tree, UpdateKind::Weak);
  m1_deep_taint_tree.write(
      Path{right}, m3_collapsed_taint_tree, UpdateKind::Weak);
  m1_deep_taint_tree.write(Path{thisn1}, m4_deep_taint_tree, UpdateKind::Weak);

  EXPECT_EQ(
      environment.deep_read(widening_resolver, m1.get()), m1_deep_taint_tree);
}

} // namespace marianatrench
