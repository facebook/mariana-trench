/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>

#include <gmock/gmock.h>

#include <sparta/PatriciaTreeSetAbstractDomain.h>

#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class TaintAccessPathTreeTest : public test::Test {};

namespace {

TaintConfig get_taint_config(const std::string& kind) {
  return TaintConfig(
      /* kind */ KindFactory::singleton().get(kind),
      /* callee_port */
      AccessPathFactory::singleton().get(AccessPath(Root(Root::Kind::Return))),
      /* callee */ nullptr,
      /* call_kind */ CallKind::declaration(),
      /* call_position */ nullptr,
      /* class_interval_context */ CallClassIntervalContext(),
      /* distance */ 0,
      /* origins */ {},
      /* inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user_features */ FeatureSet::bottom(),
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* canonical_names */ {},
      /* output_paths */ {},
      /* local_positions */ {},
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* extra_traces */ {});
}

Taint get_taint(std::initializer_list<std::string> kinds) {
  Taint result;

  for (const auto& kind : kinds) {
    result.add(get_taint_config(kind));
  }

  return result;
}

} // namespace

TEST_F(TaintAccessPathTreeTest, DefaultConstructor) {
  EXPECT_TRUE(TaintAccessPathTree().is_bottom());
}

TEST_F(TaintAccessPathTreeTest, WriteWeak) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  auto tree = TaintAccessPathTree();
  EXPECT_TRUE(tree.is_bottom());

  tree.write(
      AccessPath(Root(Root::Kind::Return)), get_taint({"1"}), UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.read(Root(Root::Kind::Return)), (TaintTree{get_taint({"1"})}));
  EXPECT_TRUE(tree.read(Root(Root::Kind::Argument, 0)).is_bottom());

  tree.write(
      AccessPath(Root(Root::Kind::Return), Path{x}),
      get_taint({"1", "2"}),
      UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)),
      (TaintTree{
          {Path{}, get_taint({"1"})},
          {Path{x}, get_taint({"2"})},
      }));
  EXPECT_TRUE(tree.read(Root(Root::Kind::Argument, 0)).is_bottom());

  tree.write(
      AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
      get_taint({"3"}),
      UpdateKind::Weak);
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)),
      (TaintTree{
          {Path{}, get_taint({"1"})},
          {Path{x}, get_taint({"2"})},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 0)),
      (TaintTree{
          {Path{y}, get_taint({"3"})},
      }));

  tree.write(
      AccessPath(Root(Root::Kind::Argument, 1)),
      get_taint({"1"}),
      UpdateKind::Weak);
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)),
      (TaintTree{
          {Path{}, get_taint({"1"})},
          {Path{x}, get_taint({"2"})},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 0)),
      (TaintTree{
          {Path{y}, get_taint({"3"})},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 1)), (TaintTree{get_taint({"1"})}));
}

TEST_F(TaintAccessPathTreeTest, WriteStrong) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  auto tree = TaintAccessPathTree();
  EXPECT_TRUE(tree.is_bottom());

  tree.write(
      AccessPath(Root(Root::Kind::Return)),
      get_taint({"1"}),
      UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.read(Root(Root::Kind::Return)), (TaintTree{get_taint({"1"})}));
  EXPECT_TRUE(tree.read(Root(Root::Kind::Argument, 0)).is_bottom());

  tree.write(
      AccessPath(Root(Root::Kind::Return), Path{x}),
      get_taint({"1", "2"}),
      UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)),
      (TaintTree{
          {Path{}, get_taint({"1"})},
          {Path{x}, get_taint({"2"})},
      }));
  EXPECT_TRUE(tree.read(Root(Root::Kind::Argument, 0)).is_bottom());

  tree.write(
      AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
      get_taint({"3"}),
      UpdateKind::Strong);
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)),
      (TaintTree{
          {Path{}, get_taint({"1"})},
          {Path{x}, get_taint({"2"})},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 0)),
      (TaintTree{
          {Path{y}, get_taint({"3"})},
      }));

  tree.write(
      AccessPath(Root(Root::Kind::Argument, 1)),
      get_taint({"1"}),
      UpdateKind::Strong);
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)),
      (TaintTree{
          {Path{}, get_taint({"1"})},
          {Path{x}, get_taint({"2"})},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 0)),
      (TaintTree{
          {Path{y}, get_taint({"3"})},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 1)), (TaintTree{get_taint({"1"})}));
}

TEST_F(TaintAccessPathTreeTest, Read) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  auto tree = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
      {AccessPath(Root(Root::Kind::Return), Path{x}), get_taint({"2"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{y}), get_taint({"3"})},
      {AccessPath(Root(Root::Kind::Argument, 1)), get_taint({"4"})},
  };
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Return))),
      (TaintTree{
          {Path{}, get_taint({"1"})},
          {Path{x}, get_taint({"2"})},
      }));
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Return), Path{x})),
      (TaintTree{get_taint({"1", "2"})}));
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Return), Path{x, y})),
      (TaintTree{get_taint({"1", "2"})}));
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Return), Path{y})),
      (TaintTree{get_taint({"1"})}));
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Argument, 0))),
      (TaintTree{
          {Path{y}, get_taint({"3"})},
      }));
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Argument, 0), Path{y})),
      (TaintTree{get_taint({"3"})}));
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Argument, 1))),
      (TaintTree{get_taint({"4"})}));

  // Read with TaintTreeConfigurationOverrides.
  TaintTreeConfigurationOverrides config_overrides{
      {TaintTreeConfigurationOverrideOptions::MaxModelHeight, 10},
      {TaintTreeConfigurationOverrideOptions::MaxModelWidth, 5},
  };

  tree.apply_config_overrides(config_overrides);
  auto tree_at_return_x_y =
      tree.read(AccessPath(Root(Root::Kind::Return), Path{x, y}));

  // Check that config_overrides are accessible for reads at any path.
  EXPECT_EQ(
      tree_at_return_x_y,
      (TaintTree({get_taint({"1", "2"})}, config_overrides)));
  EXPECT_EQ(tree_at_return_x_y.config_overrides(), config_overrides);
}

TEST_F(TaintAccessPathTreeTest, RawRead) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  auto tree = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
      {AccessPath(Root(Root::Kind::Return), Path{x}), get_taint({"2"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{y}), get_taint({"3"})},
      {AccessPath(Root(Root::Kind::Argument, 1)), get_taint({"4"})},
  };
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Return))),
      (TaintTree{
          {Path{}, get_taint({"1"})},
          {Path{x}, get_taint({"2"})},
      }));
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Return), Path{x})),
      (TaintTree{get_taint({"2"})}));
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Return), Path{x, y})),
      TaintTree::bottom());
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Return), Path{y})),
      TaintTree::bottom());
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Argument, 0))),
      (TaintTree{
          {Path{y}, get_taint({"3"})},
      }));
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Argument, 0), Path{y})),
      (TaintTree{get_taint({"3"})}));
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Argument, 1))),
      (TaintTree{get_taint({"4"})}));

  // Raw read with TaintTreeConfigurationOverrides.
  TaintTreeConfigurationOverrides config_overrides{
      {TaintTreeConfigurationOverrideOptions::MaxModelHeight, 10},
      {TaintTreeConfigurationOverrideOptions::MaxModelWidth, 5},
  };

  tree.apply_config_overrides(config_overrides);
  auto tree_at_this_y =
      tree.read(AccessPath(Root(Root::Kind::Argument, 0), Path{y}));

  // Check that config_overrides are accessible for reads at any path.
  EXPECT_EQ(tree_at_this_y, (TaintTree({get_taint({"3"})}, config_overrides)));
  EXPECT_EQ(tree_at_this_y.config_overrides(), config_overrides);
}

TEST_F(TaintAccessPathTreeTest, LessOrEqual) {
  const auto x = PathElement::field("x");

  EXPECT_TRUE(TaintAccessPathTree::bottom().leq(TaintAccessPathTree::bottom()));
  EXPECT_TRUE(TaintAccessPathTree().leq(TaintAccessPathTree::bottom()));

  EXPECT_TRUE(TaintAccessPathTree::bottom().leq(TaintAccessPathTree()));
  EXPECT_TRUE(TaintAccessPathTree().leq(TaintAccessPathTree()));

  auto tree1 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
  };
  EXPECT_FALSE(tree1.leq(TaintAccessPathTree::bottom()));
  EXPECT_FALSE(tree1.leq(TaintAccessPathTree{}));
  EXPECT_TRUE(TaintAccessPathTree::bottom().leq(tree1));
  EXPECT_TRUE(TaintAccessPathTree().leq(tree1));
  EXPECT_TRUE(tree1.leq(tree1));

  auto tree2 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1", "2"})},
  };
  EXPECT_TRUE(tree1.leq(tree2));
  EXPECT_FALSE(tree2.leq(tree1));
  EXPECT_TRUE(tree2.leq(tree2));

  auto tree3 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"2", "3"})},
  };
  EXPECT_FALSE(tree1.leq(tree3));
  EXPECT_FALSE(tree2.leq(tree3));
  EXPECT_FALSE(tree3.leq(tree1));
  EXPECT_FALSE(tree3.leq(tree2));

  auto tree4 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
      {AccessPath(Root(Root::Kind::Return), Path{x}), get_taint({"2"})},
  };
  EXPECT_TRUE(tree1.leq(tree4));
  EXPECT_FALSE(tree4.leq(tree1));
  EXPECT_FALSE(tree2.leq(tree4));
  EXPECT_TRUE(tree4.leq(tree2));
  EXPECT_FALSE(tree3.leq(tree4));
  EXPECT_FALSE(tree4.leq(tree3));

  auto tree5 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 0)), get_taint({"3"})},
  };
  EXPECT_TRUE(tree1.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree1));
  EXPECT_TRUE(tree2.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree2));
  EXPECT_FALSE(tree3.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree3));
  EXPECT_TRUE(tree4.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree4));

  auto tree6 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1", "2", "3"})},
      {AccessPath(Root(Root::Kind::Argument, 0)), get_taint({"3", "4"})},
  };
  EXPECT_TRUE(tree1.leq(tree6));
  EXPECT_FALSE(tree6.leq(tree1));
  EXPECT_TRUE(tree2.leq(tree6));
  EXPECT_FALSE(tree6.leq(tree2));
  EXPECT_TRUE(tree3.leq(tree6));
  EXPECT_FALSE(tree6.leq(tree3));
  EXPECT_TRUE(tree4.leq(tree6));
  EXPECT_FALSE(tree6.leq(tree4));
  EXPECT_TRUE(tree5.leq(tree6));
  EXPECT_FALSE(tree6.leq(tree5));

  auto tree7 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Argument, 0)), get_taint({"4"})},
  };
  EXPECT_FALSE(tree1.leq(tree7));
  EXPECT_FALSE(tree7.leq(tree1));
  EXPECT_FALSE(tree2.leq(tree7));
  EXPECT_FALSE(tree7.leq(tree2));
  EXPECT_FALSE(tree3.leq(tree7));
  EXPECT_FALSE(tree7.leq(tree3));
  EXPECT_FALSE(tree4.leq(tree7));
  EXPECT_FALSE(tree7.leq(tree4));
  EXPECT_FALSE(tree5.leq(tree7));
  EXPECT_FALSE(tree7.leq(tree5));
  EXPECT_FALSE(tree6.leq(tree7));
  EXPECT_TRUE(tree7.leq(tree6));

  // Compare trees with and without config overrides.
  TaintTreeConfigurationOverrides config_override1{
      {TaintTreeConfigurationOverrideOptions::MaxModelHeight, 10},
      {TaintTreeConfigurationOverrideOptions::MaxModelWidth, 5},
  };
  auto tree1_override1 = tree1;
  tree1_override1.apply_config_overrides(config_override1);
  EXPECT_TRUE(tree1.leq(tree1_override1));
  EXPECT_FALSE(tree1_override1.leq(tree1));

  // Compare trees with different config overrides
  TaintTreeConfigurationOverrides config_override2{
      {TaintTreeConfigurationOverrideOptions::MaxModelHeight, 20},
      {TaintTreeConfigurationOverrideOptions::MaxModelWidth, 5},
  };
  auto tree1_override2 = tree1;
  tree1_override2.apply_config_overrides(config_override2);
  EXPECT_TRUE(tree1_override1.leq(tree1_override2));
  EXPECT_FALSE(tree1_override2.leq(tree1_override1));
}

TEST_F(TaintAccessPathTreeTest, Equal) {
  const auto x = PathElement::field("x");

  EXPECT_TRUE(
      TaintAccessPathTree::bottom().equals(TaintAccessPathTree::bottom()));
  EXPECT_TRUE(TaintAccessPathTree().equals(TaintAccessPathTree::bottom()));
  EXPECT_TRUE(TaintAccessPathTree::bottom().equals(TaintAccessPathTree()));
  EXPECT_TRUE(TaintAccessPathTree().equals(TaintAccessPathTree()));

  auto tree1 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
  };
  EXPECT_FALSE(tree1.equals(TaintAccessPathTree::bottom()));
  EXPECT_FALSE(TaintAccessPathTree::bottom().equals(tree1));
  EXPECT_TRUE(tree1.equals(tree1));

  auto tree2 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1", "2"})},
  };
  EXPECT_FALSE(tree1.equals(tree2));
  EXPECT_TRUE(tree2.equals(tree2));

  auto tree3 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"2", "3"})},
  };
  EXPECT_FALSE(tree1.equals(tree3));
  EXPECT_FALSE(tree2.equals(tree3));
  EXPECT_TRUE(tree3.equals(tree3));

  auto tree4 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
      {AccessPath(Root(Root::Kind::Return), Path{x}), get_taint({"2"})},
  };
  EXPECT_FALSE(tree1.equals(tree4));
  EXPECT_FALSE(tree2.equals(tree4));
  EXPECT_FALSE(tree3.equals(tree4));
  EXPECT_TRUE(tree4.equals(tree4));

  auto tree5 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 0)), get_taint({"3"})},
  };
  EXPECT_FALSE(tree1.equals(tree5));
  EXPECT_FALSE(tree2.equals(tree5));
  EXPECT_FALSE(tree3.equals(tree5));
  EXPECT_FALSE(tree4.equals(tree5));
  EXPECT_TRUE(tree5.equals(tree5));

  auto tree6 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1", "2", "3"})},
      {AccessPath(Root(Root::Kind::Argument, 0)), get_taint({"3", "4"})},
  };
  EXPECT_FALSE(tree1.equals(tree6));
  EXPECT_FALSE(tree2.equals(tree6));
  EXPECT_FALSE(tree3.equals(tree6));
  EXPECT_FALSE(tree4.equals(tree6));
  EXPECT_FALSE(tree5.equals(tree6));
  EXPECT_TRUE(tree6.equals(tree6));

  auto tree7 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Argument, 0)), get_taint({"4"})},
  };
  EXPECT_FALSE(tree1.equals(tree7));
  EXPECT_FALSE(tree2.equals(tree7));
  EXPECT_FALSE(tree3.equals(tree7));
  EXPECT_FALSE(tree4.equals(tree7));
  EXPECT_FALSE(tree5.equals(tree7));
  EXPECT_FALSE(tree6.equals(tree7));
  EXPECT_TRUE(tree7.equals(tree7));

  // Compare trees with and without config overrides.
  TaintTreeConfigurationOverrides config_override1{
      {TaintTreeConfigurationOverrideOptions::MaxModelHeight, 10},
      {TaintTreeConfigurationOverrideOptions::MaxModelWidth, 5},
  };
  auto tree1_override1 = tree1;
  tree1_override1.apply_config_overrides(config_override1);
  EXPECT_TRUE(tree1_override1.equals(tree1_override1));
  EXPECT_FALSE(tree1_override1.equals(tree1));
  EXPECT_FALSE(tree1.equals(tree1_override1));

  // Compare trees with different config overrides
  TaintTreeConfigurationOverrides config_override2{
      {TaintTreeConfigurationOverrideOptions::MaxModelHeight, 20},
      {TaintTreeConfigurationOverrideOptions::MaxModelWidth, 5},
  };
  auto tree1_override2 = tree1;
  tree1_override2.apply_config_overrides(config_override2);
  EXPECT_TRUE(tree1_override2.equals(tree1_override2));
  EXPECT_FALSE(tree1_override1.equals(tree1_override2));
  EXPECT_FALSE(tree1_override2.equals(tree1_override1));
}

TEST_F(TaintAccessPathTreeTest, Join) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = TaintAccessPathTree::bottom();
  tree.join_with(
      TaintAccessPathTree{
          {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
      });
  EXPECT_EQ(
      tree,
      (TaintAccessPathTree{
          {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
      }));

  tree.join_with(TaintAccessPathTree::bottom());
  EXPECT_EQ(
      tree,
      (TaintAccessPathTree{
          {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
      }));

  tree.join_with(
      TaintAccessPathTree{
          {AccessPath(Root(Root::Kind::Return)), get_taint({"2"})},
      });
  EXPECT_EQ(
      tree,
      (TaintAccessPathTree{
          {AccessPath(Root(Root::Kind::Return)), get_taint({"1", "2"})},
      }));

  tree = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1", "2", "3"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
       get_taint({"3", "4"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
       get_taint({"5", "6"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}),
       get_taint({"7", "8"})},
      {AccessPath(Root(Root::Kind::Argument, 1)), get_taint({"10"})},
  };
  tree.join_with(
      TaintAccessPathTree{
          {AccessPath(Root(Root::Kind::Return), Path{x}), get_taint({"1"})},
          {AccessPath(Root(Root::Kind::Return), Path{y}), get_taint({"2"})},
          {AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
           get_taint({"6", "7"})},
          {AccessPath(Root(Root::Kind::Argument, 0), Path{x, x}),
           get_taint({"8", "9"})},
          {AccessPath(Root(Root::Kind::Argument, 2)), get_taint({"20"})},
      });
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)),
      (TaintTree{get_taint({"1", "2", "3"})}));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 0)),
      (TaintTree{
          {Path{x}, get_taint({"3", "4", "6", "7"})},
          {Path{x, y}, get_taint({"5"})},
          {Path{x, z}, get_taint({"8"})},
          {Path{x, x}, get_taint({"8", "9"})},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 1)), (TaintTree{get_taint({"10"})}));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 2)), (TaintTree{get_taint({"20"})}));
  EXPECT_TRUE(tree.read(Root(Root::Kind::Argument, 3)).is_bottom());

  // Join trees with and without config overrides.
  TaintAccessPathTree tree1{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
  };
  TaintTreeConfigurationOverrides config_override1{
      {TaintTreeConfigurationOverrideOptions::MaxModelHeight, 10},
      {TaintTreeConfigurationOverrideOptions::MaxModelWidth, 5},
  };
  auto tree1_override1 = tree1;
  tree1_override1.apply_config_overrides(config_override1);
  EXPECT_TRUE(tree1.config_overrides(Root(Root::Kind::Return)).is_bottom());
  EXPECT_FALSE(
      tree1_override1.config_overrides(Root(Root::Kind::Return)).is_bottom());

  tree1.join_with(tree1_override1);

  EXPECT_FALSE(tree1.config_overrides(Root(Root::Kind::Return)).is_bottom());
  EXPECT_EQ(
      tree1.config_overrides(Root(Root::Kind::Return)),
      tree1_override1.config_overrides(Root(Root::Kind::Return)));

  EXPECT_EQ(
      tree1.read(Root(Root::Kind::Return)),
      (TaintTree(get_taint({"1"}), config_override1)));

  // Join trees with different config overrides
  TaintTreeConfigurationOverrides config_override2{
      {TaintTreeConfigurationOverrideOptions::MaxModelHeight, 5},
      {TaintTreeConfigurationOverrideOptions::MaxModelWidth, 7},
  };
  auto tree2 = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"2"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
       get_taint({"3", "4"})}};
  auto tree2_override2 = tree2;
  tree2_override2.apply_config_overrides(config_override2);

  tree1.join_with(tree2);
  EXPECT_EQ(
      tree1.read(Root(Root::Kind::Return)),
      (TaintTree(get_taint({"1", "2"}), config_override1)));
  // No config overrides for Argument(0) yet.
  EXPECT_EQ(
      tree1.read(AccessPath(Root(Root::Kind::Argument, 0), Path{x, y})),
      (TaintTree(get_taint({"3", "4"}))));

  tree1.join_with(tree2_override2);
  // Config overrides for Return is the join of config_override1 and
  // config_override2.
  EXPECT_EQ(
      tree1.read(Root(Root::Kind::Return)),
      (TaintTree(
          get_taint({"1", "2"}),
          TaintTreeConfigurationOverrides{
              {TaintTreeConfigurationOverrideOptions::MaxModelHeight, 10},
              {TaintTreeConfigurationOverrideOptions::MaxModelWidth, 7},
          })));
  // Config overrides for Argument(0) is config_override2
  EXPECT_EQ(
      tree1.read(AccessPath(Root(Root::Kind::Argument, 0), Path{x, y})),
      (TaintTree(get_taint({"3", "4"}), config_override2)));
}

TEST_F(TaintAccessPathTreeTest, Elements) {
  using Pair = std::pair<AccessPath, Taint>;

  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = TaintAccessPathTree::bottom();
  EXPECT_TRUE(tree.elements().empty());

  tree = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
  };
  EXPECT_THAT(
      tree.elements(),
      testing::UnorderedElementsAre(
          Pair{AccessPath(Root(Root::Kind::Return)), get_taint({"1"})}));

  tree = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
       get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
       get_taint({"3", "4"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}),
       get_taint({"5", "6"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z, y}),
       get_taint({"7", "8"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, x}),
       get_taint({"9", "10"})},
      {AccessPath(Root(Root::Kind::Argument, 1), Path{x, y}),
       get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 2)), get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 2), Path{x, y}),
       get_taint({"3", "4"})},
  };
  EXPECT_THAT(
      tree.elements(),
      testing::UnorderedElementsAre(
          Pair{AccessPath(Root(Root::Kind::Return)), get_taint({"1", "2"})},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
              get_taint({"1", "2"})},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
              get_taint({"3", "4"})},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}),
              get_taint({"5", "6"})},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 0), Path{x, z, y}),
              get_taint({"7", "8"})},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 0), Path{x, x}),
              get_taint({"9", "10"})},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 1), Path{x, y}),
              get_taint({"1", "2"})},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 2)), get_taint({"1", "2"})},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 2), Path{x, y}),
              get_taint({"3", "4"})}));
}

TEST_F(TaintAccessPathTreeTest, Transform) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
       get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
       get_taint({"3", "4"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}),
       get_taint({"5", "6"})},
      {AccessPath(Root(Root::Kind::Argument, 1), Path{x, y}),
       get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 2)), get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 2), Path{x, y}),
       get_taint({"3", "4"})},
  };
  tree.transform([](const Taint& taint) {
    Taint result{};
    taint.visit_frames([&result](const CallInfo&, const Frame& frame) {
      auto kind_int = std::atoi(frame.kind()->as<NamedKind>()->name().c_str());

      result.add(get_taint_config({std::to_string(kind_int * kind_int)}));
    });

    return result;
  });
  EXPECT_EQ(
      tree,
      (TaintAccessPathTree{
          {AccessPath(Root(Root::Kind::Return)), get_taint({"1", "4"})},
          {AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
           get_taint({"1", "4"})},
          {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
           get_taint({"9", "16"})},
          {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}),
           get_taint({"25", "36"})},
          {AccessPath(Root(Root::Kind::Argument, 1), Path{x, y}),
           get_taint({"1", "4"})},
          {AccessPath(Root(Root::Kind::Argument, 2)), get_taint({"1", "4"})},
          {AccessPath(Root(Root::Kind::Argument, 2), Path{x, y}),
           get_taint({"9", "16"})},
      }));

  // Transform tree with config overrides
  auto tree_override = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
       get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
       get_taint({"3", "4"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}),
       get_taint({"5", "6"})},
      {AccessPath(Root(Root::Kind::Argument, 1), Path{x, y}),
       get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 2)), get_taint({"1", "2"})},
      {AccessPath(Root(Root::Kind::Argument, 2), Path{x, y}),
       get_taint({"3", "4"})},
  };
  TaintTreeConfigurationOverrides config_override{
      {TaintTreeConfigurationOverrideOptions::MaxModelHeight, 10},
      {TaintTreeConfigurationOverrideOptions::MaxModelWidth, 5},
  };
  tree_override.apply_config_overrides(config_override);

  tree_override.transform([](const Taint& taint) {
    Taint result{};
    taint.visit_frames([&result](const CallInfo&, const Frame& frame) {
      auto kind_int = std::atoi(frame.kind()->as<NamedKind>()->name().c_str());

      result.add(get_taint_config({std::to_string(kind_int * kind_int)}));
    });

    return result;
  });

  TaintAccessPathTree expected_tree = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1", "4"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
       get_taint({"1", "4"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
       get_taint({"9", "16"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}),
       get_taint({"25", "36"})},
      {AccessPath(Root(Root::Kind::Argument, 1), Path{x, y}),
       get_taint({"1", "4"})},
      {AccessPath(Root(Root::Kind::Argument, 2)), get_taint({"1", "4"})},
      {AccessPath(Root(Root::Kind::Argument, 2), Path{x, y}),
       get_taint({"9", "16"})},
  };
  expected_tree.apply_config_overrides(config_override);
  EXPECT_EQ(tree_override, expected_tree);
}

TEST_F(TaintAccessPathTreeTest, CollapseInvalid) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}), get_taint({"2"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}), get_taint({"3"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}), get_taint({"4"})},
      {AccessPath(Root(Root::Kind::Argument, 1)), get_taint({"5"})},
      {AccessPath(Root(Root::Kind::Argument, 1), Path{x}), get_taint({"6"})},
  };

  using Accumulator = std::string;

  // Invalid paths are all children of "x", but "x" itself is valid.
  auto is_valid = [](const Accumulator& previous_field,
                     Path::Element path_element) {
    if (previous_field == "x") {
      return std::make_pair(false, std::string(""));
    }
    return std::make_pair(true, path_element.name()->str_copy());
  };

  // Argument(1) will be an invalid root, return an accumulator that causes
  // its children to be collapsed under `is_valid` above. The argument itself
  // will still exist.
  auto initial_accumulator = [](const Root& root) {
    if (root.is_argument() && root.parameter_position() == 1) {
      return std::string("x");
    }
    return root.to_string();
  };

  tree.collapse_invalid_paths<Accumulator>(
      is_valid,
      initial_accumulator,
      /* broadening features */ FeatureMayAlwaysSet{});
  EXPECT_EQ(
      tree,
      (TaintAccessPathTree{
          {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
          {AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
           get_taint({"2", "3", "4"})},
          {AccessPath(Root(Root::Kind::Argument, 1)), get_taint({"5", "6"})},
      }));

  // Collapse with config override still holds the overrides
  auto tree_override = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}), get_taint({"2"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}), get_taint({"3"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}), get_taint({"4"})},
      {AccessPath(Root(Root::Kind::Argument, 1)), get_taint({"5"})},
      {AccessPath(Root(Root::Kind::Argument, 1), Path{x}), get_taint({"6"})},
  };
  TaintTreeConfigurationOverrides config_override{
      {TaintTreeConfigurationOverrideOptions::MaxModelHeight, 10},
      {TaintTreeConfigurationOverrideOptions::MaxModelWidth, 5},
  };
  tree_override.apply_config_overrides(config_override);

  tree_override.collapse_invalid_paths<Accumulator>(
      is_valid,
      initial_accumulator,
      /* broadening features */ FeatureMayAlwaysSet{});
  TaintAccessPathTree expected_tree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
       get_taint({"2", "3", "4"})},
      {AccessPath(Root(Root::Kind::Argument, 1)), get_taint({"5", "6"})},
  };
  expected_tree.apply_config_overrides(config_override);
  EXPECT_EQ(tree_override, expected_tree);
}

TEST_F(TaintAccessPathTreeTest, ShapeWith) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}), get_taint({"2"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}), get_taint({"3"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}), get_taint({"4"})},
  };
  TaintTreeConfigurationOverrides config_override{
      {TaintTreeConfigurationOverrideOptions::MaxModelHeight, 5},
      {TaintTreeConfigurationOverrideOptions::MaxModelWidth, 10},
  };
  tree.apply_config_overrides(config_override);
  // Add new root Argument(1) without config overrides
  tree.join_with(
      TaintAccessPathTree{
          {AccessPath(Root(Root::Kind::Argument, 1)), get_taint({"5"})},
          {AccessPath(Root(Root::Kind::Argument, 1), Path{x}),
           get_taint({"6"})},
      });
  EXPECT_TRUE(tree.config_overrides(Root(Root::Kind::Argument, 1)).is_bottom());

  // Dummy mold to only keep certain kinds
  std::unordered_set<const Kind*> keep_kinds = {
      KindFactory::singleton().get("1"),
      KindFactory::singleton().get("2"),
      KindFactory::singleton().get("3")};
  const auto make_mold = [&keep_kinds](Taint taint) {
    if (std::any_of(
            keep_kinds.begin(), keep_kinds.end(), [&taint](const Kind* kind) {
              return taint.contains_kind(kind);
            })) {
      return taint.essential();
    } else {
      return Taint::bottom();
    }
  };

  tree.shape_with(make_mold, /* broadening features */ FeatureMayAlwaysSet{});

  auto expected_tree = TaintAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), get_taint({"1"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
       get_taint({"2", "4"})},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}), get_taint({"3"})},
  };
  expected_tree.apply_config_overrides(config_override);
  expected_tree.join_with(
      TaintAccessPathTree{
          {AccessPath(Root(Root::Kind::Argument, 1)), get_taint({"5", "6"})},
      });

  EXPECT_FALSE(tree.config_overrides(Root(Root::Kind::Return)).is_bottom());
  EXPECT_EQ(tree.config_overrides(Root(Root::Kind::Return)), config_override);
  EXPECT_FALSE(
      tree.config_overrides(Root(Root::Kind::Argument, 0)).is_bottom());
  EXPECT_EQ(
      tree.config_overrides(Root(Root::Kind::Argument, 0)), config_override);
  EXPECT_TRUE(tree.config_overrides(Root(Root::Kind::Argument, 1)).is_bottom());
  EXPECT_EQ(tree, expected_tree);
}

} // namespace marianatrench
