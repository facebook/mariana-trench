/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <PatriciaTreeSetAbstractDomain.h>
#include <Show.h>

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/TaintTree.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

namespace {

Taint make_artificial_source(ParameterPosition parameter_position) {
  return Taint::artificial_source(
      AccessPath(Root(Root::Kind::Argument, parameter_position)));
}

} // namespace

class AbstractTreeDomainTest : public test::Test {};

using IntSet = sparta::PatriciaTreeSetAbstractDomain<unsigned>;
using IntSetTree = AbstractTreeDomain<IntSet>;

TEST_F(AbstractTreeDomainTest, PathElementMapIterator) {
  using Map = sparta::PatriciaTreeMap<PathElement::ElementEncoding, IntSet>;

  const auto field_element = PathElement::field("field");
  const auto index_element = PathElement::index("index");
  const auto any_index_element = PathElement::any_index();

  auto map = Map();
  // Insert using PathElement::ElementEncoding
  map.insert_or_assign(field_element.encode(), IntSet{1});
  map.insert_or_assign(index_element.encode(), IntSet{2});
  map.insert_or_assign(any_index_element.encode(), IntSet{3});

  // Iterate
  for (const auto& [path_element, value] : PathElementMapIterator(map)) {
    switch (path_element.kind()) {
      case PathElement::Kind::Field:
        EXPECT_TRUE(path_element.is_field());
        EXPECT_EQ(path_element.str(), ".field");
        EXPECT_EQ(value, IntSet{1});
        break;
      case PathElement::Kind::Index:
        EXPECT_TRUE(path_element.is_index());
        EXPECT_EQ(path_element.str(), "[index]");
        EXPECT_EQ(value, IntSet{2});
        break;
      case PathElement::Kind::AnyIndex:
        EXPECT_TRUE(path_element.is_any_index());
        EXPECT_EQ(path_element.str(), "[*]");
        EXPECT_EQ(value, IntSet{3});
        break;
    }
  }
}

TEST_F(AbstractTreeDomainTest, DefaultConstructor) {
  EXPECT_TRUE(IntSetTree().is_bottom());
  EXPECT_TRUE(IntSetTree().root().is_bottom());
  EXPECT_TRUE(IntSetTree().successors().empty());
}

TEST_F(AbstractTreeDomainTest, WriteElementsWeak) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetTree{IntSet{1}};
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), IntSet{1});
  EXPECT_TRUE(tree.successors().empty());
  EXPECT_TRUE(tree.successor(x).is_bottom());

  tree.write(Path{}, IntSet{2}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1, 2}));
  EXPECT_TRUE(tree.successors().empty());

  tree.write(Path{x}, IntSet{3, 4}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1, 2}));
  EXPECT_EQ(tree.successors().size(), 1);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{3, 4}));
  EXPECT_TRUE(tree.successor(x).successors().empty());

  tree.write(Path{y}, IntSet{5, 6}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1, 2}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x), (IntSetTree{IntSet{3, 4}}));
  EXPECT_EQ(tree.successor(y), (IntSetTree{IntSet{5, 6}}));

  // Ignore elements already present on the root.
  tree.write(Path{y}, IntSet{2}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1, 2}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x), (IntSetTree{IntSet{3, 4}}));
  EXPECT_EQ(tree.successor(y), (IntSetTree{IntSet{5, 6}}));

  // Ignore elements already present on the path.
  tree.write(Path{x, z}, IntSet{4}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1, 2}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x), (IntSetTree{IntSet{3, 4}}));
  EXPECT_EQ(tree.successor(y), (IntSetTree{IntSet{5, 6}}));

  // Ignore elements already present on the path, within different nodes.
  tree.write(Path{x, z}, IntSet{1, 3}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1, 2}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x), (IntSetTree{IntSet{3, 4}}));
  EXPECT_EQ(tree.successor(y), (IntSetTree{IntSet{5, 6}}));

  tree.write(Path{x, z}, IntSet{1, 3, 5, 7}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1, 2}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{3, 4}));
  EXPECT_EQ(tree.successor(x).successors().size(), 1);
  EXPECT_EQ(tree.successor(x).successor(z), (IntSetTree{IntSet{5, 7}}));
  EXPECT_EQ(tree.successor(y), (IntSetTree{IntSet{5, 6}}));

  // Children are pruned.
  tree.write(Path{x}, IntSet{5, 9, 10}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1, 2}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{3, 4, 5, 9, 10}));
  EXPECT_EQ(tree.successor(x).successors().size(), 1);
  EXPECT_EQ(tree.successor(x).successor(z), (IntSetTree{IntSet{7}}));
  EXPECT_EQ(tree.successor(y), (IntSetTree{IntSet{5, 6}}));

  // Newly introduced nodes are set to bottom.
  tree = IntSetTree{{Path{x, y}, IntSet{1}}};
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_TRUE(tree.root().is_bottom());
  EXPECT_EQ(tree.successors().size(), 1);
  EXPECT_TRUE(tree.successor(x).root().is_bottom());
  EXPECT_EQ(tree.successor(x).successors().size(), 1);
  EXPECT_EQ(tree.successor(x).successor(y), (IntSetTree{IntSet{1}}));
}

TEST_F(AbstractTreeDomainTest, WriteIndexElementsWeak) {
  const auto x = PathElement::field("x");
  const auto xi = PathElement::index("x");
  const auto yi = PathElement::index("y");
  const auto ai = PathElement::any_index();

  auto tree = IntSetTree{IntSet{1}};
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), IntSet{1});
  EXPECT_TRUE(tree.successors().empty());
  EXPECT_TRUE(tree.successor(x).is_bottom());

  tree.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 1);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_TRUE(tree.successor(x).successors().empty());

  tree.write(Path{xi}, IntSet{3}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{3}));
  EXPECT_TRUE(tree.successor(xi).successors().empty());

  // Write to [*] implies writes to all existing indices
  tree.write(Path{ai}, IntSet{4}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 3);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{3, 4}));
  EXPECT_EQ(tree.successor(ai).root(), (IntSet{4}));
  EXPECT_TRUE(tree.successor(xi).successors().empty());
  EXPECT_TRUE(tree.successor(ai).successors().empty());

  // Write to a new index [y] on a tree with existing [*] implies
  // [y] inherits from exiting [*]
  tree.write(Path{yi}, IntSet{5}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 4);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{3, 4}));
  EXPECT_EQ(tree.successor(yi).root(), (IntSet{4, 5}));
  EXPECT_EQ(tree.successor(ai).root(), (IntSet{4}));
  EXPECT_TRUE(tree.successor(xi).successors().empty());
  EXPECT_TRUE(tree.successor(yi).successors().empty());
  EXPECT_TRUE(tree.successor(ai).successors().empty());

  // Write to children of [*] implies writes to children
  // of all existing indices
  tree.write(Path{ai, xi}, IntSet{6}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 4);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{3, 4}));
  EXPECT_EQ(tree.successor(yi).root(), (IntSet{4, 5}));
  EXPECT_EQ(tree.successor(ai).root(), (IntSet{4}));
  EXPECT_EQ(tree.successor(xi).successors().size(), 1);
  EXPECT_EQ(tree.successor(yi).successors().size(), 1);
  EXPECT_EQ(tree.successor(ai).successors().size(), 1);
  EXPECT_EQ(tree.successor(xi).successor(xi).root(), (IntSet{6}));
  EXPECT_EQ(tree.successor(yi).successor(xi).root(), (IntSet{6}));
  EXPECT_EQ(tree.successor(ai).successor(xi).root(), (IntSet{6}));

  // Ignore elements already present on the ancestor.
  tree.write(Path{xi, xi}, IntSet{1}, UpdateKind::Weak);
  tree.write(Path{yi, xi}, IntSet{4}, UpdateKind::Weak);
  tree.write(Path{ai, xi}, IntSet{4}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 4);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{3, 4}));
  EXPECT_EQ(tree.successor(yi).root(), (IntSet{4, 5}));
  EXPECT_EQ(tree.successor(ai).root(), (IntSet{4}));
  EXPECT_EQ(tree.successor(xi).successors().size(), 1);
  EXPECT_EQ(tree.successor(yi).successors().size(), 1);
  EXPECT_EQ(tree.successor(ai).successors().size(), 1);
  EXPECT_EQ(tree.successor(xi).successor(xi).root(), (IntSet{6}));
  EXPECT_EQ(tree.successor(yi).successor(xi).root(), (IntSet{6}));
  EXPECT_EQ(tree.successor(ai).successor(xi).root(), (IntSet{6}));

  // Children of index is pruned.
  tree.write(Path{xi}, IntSet{6}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 4);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{3, 4, 6}));
  EXPECT_EQ(tree.successor(yi).root(), (IntSet{4, 5}));
  EXPECT_EQ(tree.successor(ai).root(), (IntSet{4}));
  EXPECT_EQ(tree.successor(xi).successor(xi).root(), (IntSet{}));
  EXPECT_EQ(tree.successor(yi).successor(xi).root(), (IntSet{6}));
  EXPECT_EQ(tree.successor(ai).successor(xi).root(), (IntSet{6}));

  // Write to [*] also prunes the children of sibling indices
  tree.write(Path{ai}, IntSet{6}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 4);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{3, 4, 6}));
  EXPECT_EQ(tree.successor(yi).root(), (IntSet{4, 5, 6}));
  EXPECT_EQ(tree.successor(ai).root(), (IntSet{4, 6}));
  EXPECT_EQ(tree.successor(xi).successor(xi).root(), (IntSet{}));
  EXPECT_EQ(tree.successor(yi).successor(xi).root(), (IntSet{}));
  EXPECT_EQ(tree.successor(ai).successor(xi).root(), (IntSet{}));

  // Children are pruned including indices and [*]
  tree.write(Path{}, IntSet{3, 4, 5, 6}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1, 3, 4, 5, 6}));
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
}

TEST_F(AbstractTreeDomainTest, WriteElementsStrong) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetTree{IntSet{1}};
  tree.write(Path{}, IntSet{2}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{2}));
  EXPECT_TRUE(tree.successors().empty());

  tree.write(Path{x}, IntSet{3, 4}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{2}));
  EXPECT_EQ(tree.successors().size(), 1);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{3, 4}));
  EXPECT_TRUE(tree.successor(x).successors().empty());

  tree.write(Path{y}, IntSet{5, 6}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{2}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x), (IntSetTree{IntSet{3, 4}}));
  EXPECT_EQ(tree.successor(y), (IntSetTree{IntSet{5, 6}}));

  // Ignore elements already present on the root.
  tree.write(Path{y}, IntSet{2}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{2}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x), (IntSetTree{IntSet{3, 4}}));
  EXPECT_EQ(tree.successor(y), (IntSetTree{IntSet{}}));

  // Ignore elements already present on the path.
  tree.write(Path{x, z}, IntSet{4}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{2}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x), (IntSetTree{IntSet{3, 4}}));
  EXPECT_EQ(tree.successor(y), (IntSetTree{IntSet{}}));

  // Ignore elements already present on the path, within different nodes.
  tree.write(Path{x, z}, IntSet{2, 3}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{2}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x), (IntSetTree{IntSet{3, 4}}));
  EXPECT_EQ(tree.successor(y), (IntSetTree{IntSet{}}));

  tree.write(Path{x, z}, IntSet{2, 3, 5, 7}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{2}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{3, 4}));
  EXPECT_EQ(tree.successor(x).successors().size(), 1);
  EXPECT_EQ(tree.successor(x).successor(z), (IntSetTree{IntSet{5, 7}}));
  EXPECT_EQ(tree.successor(y), (IntSetTree{IntSet{}}));

  // Strong writes remove all children.
  tree.write(Path{x}, IntSet{3}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{2}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{3}));
  EXPECT_EQ(tree.successor(x).successors().size(), 0);
  EXPECT_EQ(tree.successor(y), (IntSetTree{IntSet{}}));
}

TEST_F(AbstractTreeDomainTest, WriteIndexElementsStrong) {
  const auto x = PathElement::field("x");
  const auto xi = PathElement::index("x");
  const auto yi = PathElement::index("y");
  const auto ai = PathElement::any_index();

  auto tree = IntSetTree{IntSet{1}};
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), IntSet{1});
  EXPECT_TRUE(tree.successors().empty());
  EXPECT_TRUE(tree.successor(x).is_bottom());

  tree.write(Path{x}, IntSet{2}, UpdateKind::Strong);
  tree.write(Path{xi}, IntSet{2}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{2}));
  EXPECT_TRUE(tree.successor(x).successors().empty());
  EXPECT_TRUE(tree.successor(xi).successors().empty());

  tree.write(Path{xi}, IntSet{3}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 2);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{3}));
  EXPECT_TRUE(tree.successor(x).successors().empty());
  EXPECT_TRUE(tree.successor(xi).successors().empty());

  // There are not strong writes to [*].
  // Write to [*] implies weak writes to all existing indices and [*]
  tree.write(Path{ai}, IntSet{4}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 3);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{3, 4}));
  EXPECT_EQ(tree.successor(ai).root(), (IntSet{4}));
  EXPECT_TRUE(tree.successor(xi).successors().empty());
  EXPECT_TRUE(tree.successor(ai).successors().empty());

  // Write to index [yi] on a tree with existing [*] does not inherit from [*]
  tree.write(Path{yi}, IntSet{5}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 4);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{3, 4}));
  EXPECT_EQ(tree.successor(yi).root(), (IntSet{5}));
  EXPECT_EQ(tree.successor(ai).root(), (IntSet{4}));

  // Write to children of [*] implies writes to children
  // of all existing indices
  tree.write(Path{ai, yi}, IntSet{6}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 4);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{3, 4}));
  EXPECT_EQ(tree.successor(yi).root(), (IntSet{5}));
  EXPECT_EQ(tree.successor(ai).root(), (IntSet{4}));
  EXPECT_EQ(tree.successor(xi).successors().size(), 1);
  EXPECT_EQ(tree.successor(yi).successors().size(), 1);
  EXPECT_EQ(tree.successor(ai).successors().size(), 1);
  EXPECT_EQ(tree.successor(xi).successor(yi).root(), (IntSet{6}));
  EXPECT_EQ(tree.successor(yi).successor(yi).root(), (IntSet{6}));
  EXPECT_EQ(tree.successor(ai).successor(yi).root(), (IntSet{6}));

  // Ignore elements already present on the ancestor.
  tree.write(Path{xi, yi}, IntSet{1}, UpdateKind::Strong);
  tree.write(Path{yi, yi}, IntSet{5}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 4);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{3, 4}));
  EXPECT_EQ(tree.successor(yi).root(), (IntSet{5}));
  EXPECT_EQ(tree.successor(ai).root(), (IntSet{4}));
  EXPECT_EQ(tree.successor(xi).successors().size(), 1);
  EXPECT_EQ(tree.successor(yi).successors().size(), 1);
  EXPECT_EQ(tree.successor(ai).successors().size(), 1);
  EXPECT_EQ(tree.successor(xi).successor(yi).root(), (IntSet{}));
  EXPECT_EQ(tree.successor(yi).successor(yi).root(), (IntSet{}));
  EXPECT_EQ(tree.successor(ai).successor(yi).root(), (IntSet{6}));

  // Children of index are removed.
  tree.write(Path{xi}, IntSet{6}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.root(), (IntSet{1}));
  EXPECT_EQ(tree.successors().size(), 4);
  EXPECT_EQ(tree.successor(x).root(), (IntSet{2}));
  EXPECT_EQ(tree.successor(xi).root(), (IntSet{6}));
  EXPECT_EQ(tree.successor(yi).root(), (IntSet{5}));
  EXPECT_EQ(tree.successor(ai).root(), (IntSet{4}));
  EXPECT_EQ(tree.successor(xi).successors().size(), 0);
  EXPECT_EQ(tree.successor(yi).successor(yi).root(), (IntSet{}));
  EXPECT_EQ(tree.successor(ai).successor(yi).root(), (IntSet{6}));
}

TEST_F(AbstractTreeDomainTest, WriteTreeWeak) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetTree{
      {Path{}, IntSet{1}},
      {Path{x}, IntSet{3}},
      {Path{x, x}, IntSet{5}},
      {Path{x, y}, IntSet{7}},
      {Path{x, z}, IntSet{9}},
      {Path{x, z, x}, IntSet{11}},
      {Path{x, z, x, x}, IntSet{13}},
      {Path{y}, IntSet{20}},
      {Path{y, x}, IntSet{22}},
  };

  // Test writes at the root.
  auto tree1 = tree;
  tree1.write(Path{}, IntSetTree{IntSet{3, 7, 11, 13, 22}}, UpdateKind::Weak);
  EXPECT_EQ(
      tree1,
      (IntSetTree{
          {Path{}, IntSet{1, 3, 7, 11, 13, 22}},
          {Path{x, x}, IntSet{5}},
          {Path{x, z}, IntSet{9}},
          {Path{y}, IntSet{20}},
      }));

  auto tree2 = tree;
  tree2.write(
      Path{},
      IntSetTree{
          {Path{}, IntSet{2}},
          {Path{x}, IntSet{4}},
          {Path{x, x}, IntSet{6}},
          {Path{x, z}, IntSet{9, 10, 11}},
          {Path{y}, IntSet{20, 21}},
      },
      UpdateKind::Weak);
  EXPECT_EQ(
      tree2,
      (IntSetTree{
          {Path{}, IntSet{1, 2}},
          {Path{x}, IntSet{3, 4}},
          {Path{x, x}, IntSet{5, 6}},
          {Path{x, y}, IntSet{7}},
          {Path{x, z}, IntSet{9, 10, 11}},
          {Path{x, z, x, x}, IntSet{13}},
          {Path{y}, IntSet{20, 21}},
          {Path{y, x}, IntSet{22}},
      }));

  // Test write at height 1.
  auto tree3 = tree;
  tree3.write(
      Path{x},
      IntSetTree{
          {Path{}, IntSet{1, 2}},
          {Path{x}, IntSet{6}},
          {Path{y}, IntSet{8}},
          {Path{z, x}, IntSet{11, 12}},
          {Path{z, x, x}, IntSet{3, 14}},
      },
      UpdateKind::Weak);
  EXPECT_EQ(
      tree3,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2, 3}},
          {Path{x, x}, IntSet{5, 6}},
          {Path{x, y}, IntSet{7, 8}},
          {Path{x, z}, IntSet{9}},
          {Path{x, z, x}, IntSet{11, 12}},
          {Path{x, z, x, x}, IntSet{13, 14}},
          {Path{y}, IntSet{20}},
          {Path{y, x}, IntSet{22}},
      }));
}

TEST_F(AbstractTreeDomainTest, WriteTreeWithIndexWeak) {
  const auto x = PathElement::field("x");
  const auto xi = PathElement::index("x");
  const auto yi = PathElement::index("y");
  const auto ai = PathElement::any_index();

  auto tree = IntSetTree{
      {Path{}, IntSet{1}},
      {Path{x}, IntSet{2}},
      {Path{xi, xi}, IntSet{3}},
      {Path{xi, yi}, IntSet{4}},
      {Path{xi, ai}, IntSet{5}},
      {Path{xi, yi, xi, yi}, IntSet{6}},
      {Path{yi}, IntSet{7}},
      {Path{yi, xi}, IntSet{8}},
  };

  // Write to [*] implies write to all existing indices
  auto tree1 = tree;
  tree1.write(Path{ai, xi}, IntSetTree{IntSet{9}}, UpdateKind::Weak);
  EXPECT_EQ(
      tree1,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
          {Path{xi, xi}, IntSet{3, 9}},
          {Path{xi, yi}, IntSet{4}},
          {Path{xi, ai}, IntSet{5}},
          {Path{ai, xi}, IntSet{9}},
          {Path{xi, yi, xi, yi}, IntSet{6}},
          {Path{yi}, IntSet{7}},
          {Path{yi, xi}, IntSet{8, 9}},
      }));

  // Write to ancestor prunes empty children
  auto tree2 = tree;
  tree2.write(Path{xi}, IntSetTree{IntSet{3, 5, 6}}, UpdateKind::Weak);
  EXPECT_EQ(
      tree2,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
          {Path{xi}, IntSet{3, 5, 6}},
          {Path{xi, yi}, IntSet{4}},
          {Path{yi}, IntSet{7}},
          {Path{yi, xi}, IntSet{8}},
      }));

  // Write to [*] implies write to all existing indices.
  // All subtrees are collapsed. But indices do not collapse into [*]
  auto tree3 = tree;
  tree3.write(
      Path{},
      IntSetTree{
          {Path{}, IntSet{2}},
          {Path{ai}, IntSet{3, 4, 5, 6, 7, 8}},
      },
      UpdateKind::Weak);
  EXPECT_EQ(
      tree3,
      (IntSetTree{
          {Path{}, IntSet{1, 2}},
          {Path{xi}, IntSet{3, 4, 5, 6, 7, 8}},
          {Path{yi}, IntSet{3, 4, 5, 6, 7, 8}},
          {Path{ai}, IntSet{3, 4, 5, 6, 7, 8}},
      }));
}

TEST_F(AbstractTreeDomainTest, WriteTreeStrong) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetTree{
      {Path{}, IntSet{1}},
      {Path{x}, IntSet{3}},
      {Path{x, x}, IntSet{5}},
      {Path{x, y}, IntSet{7}},
      {Path{x, z}, IntSet{9}},
      {Path{x, z, x}, IntSet{11}},
      {Path{x, z, x, x}, IntSet{13}},
      {Path{y}, IntSet{20}},
      {Path{y, x}, IntSet{22}},
  };

  // Test writes at the root.
  auto tree1 = tree;
  tree1.write(Path{}, IntSetTree{IntSet{30}}, UpdateKind::Strong);
  EXPECT_EQ(tree1, IntSetTree{IntSet{30}});

  auto tree2 = tree;
  tree2.write(
      Path{},
      IntSetTree{
          {Path{}, IntSet{2}},
          {Path{x}, IntSet{4}},
          {Path{y}, IntSet{6}},
      },
      UpdateKind::Strong);
  EXPECT_EQ(
      tree2,
      (IntSetTree{
          {Path{}, IntSet{2}},
          {Path{x}, IntSet{4}},
          {Path{y}, IntSet{6}},
      }));

  // Test write at height 1.
  auto tree3 = tree;
  tree3.write(
      Path{x},
      IntSetTree{
          {Path{}, IntSet{1, 2}},
          {Path{x}, IntSet{6}},
          {Path{y}, IntSet{8}},
          {Path{z, x}, IntSet{11, 12}},
          {Path{z, x, x}, IntSet{3, 14}},
      },
      UpdateKind::Strong);
  EXPECT_EQ(
      tree3,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
          {Path{x, x}, IntSet{6}},
          {Path{x, y}, IntSet{8}},
          {Path{x, z, x}, IntSet{11, 12}},
          {Path{x, z, x, x}, IntSet{3, 14}},
          {Path{y}, IntSet{20}},
          {Path{y, x}, IntSet{22}},
      }));
}

TEST_F(AbstractTreeDomainTest, WriteTreeWithIndexStrong) {
  const auto x = PathElement::field("x");
  const auto xi = PathElement::index("x");
  const auto yi = PathElement::index("y");
  const auto ai = PathElement::any_index();

  auto tree = IntSetTree{
      {Path{}, IntSet{1}},
      {Path{x}, IntSet{2}},
      {Path{xi, xi}, IntSet{3}},
      {Path{xi, yi}, IntSet{4}},
      {Path{xi, ai}, IntSet{5}},
      {Path{xi, yi, xi, yi}, IntSet{6}},
      {Path{yi}, IntSet{7}},
      {Path{yi, xi}, IntSet{8}},
  };

  // Test writes at the root.
  auto tree1 = tree;
  tree1.write(Path{}, IntSetTree{IntSet{99}}, UpdateKind::Strong);
  EXPECT_EQ(tree1, IntSetTree{IntSet{99}});

  auto tree2 = tree;
  tree2.write(
      Path{},
      IntSetTree{
          {Path{}, IntSet{2}},
          {Path{xi}, IntSet{3}},
          {Path{ai}, IntSet{4}},
      },
      UpdateKind::Strong);
  EXPECT_EQ(
      tree2,
      (IntSetTree{
          {Path{}, IntSet{2}},
          {Path{xi}, IntSet{3}},
          {Path{ai}, IntSet{4}},
      }));

  // Test write at height 1.
  auto tree3 = tree;
  tree3.write(
      Path{xi},
      IntSetTree{
          {Path{}, IntSet{1, 2}},
          {Path{xi}, IntSet{6}},
          {Path{yi}, IntSet{10}},
          {Path{yi, xi}, IntSet{11, 12}},
          {Path{ai, xi}, IntSet{13}},
      },
      UpdateKind::Strong);
  EXPECT_EQ(
      tree3,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
          {Path{xi}, IntSet{2}},
          {Path{xi, xi}, IntSet{6}},
          {Path{xi, xi, xi}, IntSet{13}},
          {Path{xi, yi}, IntSet{10}},
          {Path{xi, yi, xi}, IntSet{11, 12, 13}},
          {Path{xi, ai, xi}, IntSet{13}},
          {Path{yi}, IntSet{7}},
          {Path{yi, xi}, IntSet{8}},
      }));

  // Test write to [*]
  auto tree4 = tree;
  tree4.write(
      Path{ai},
      IntSetTree{
          {Path{}, IntSet{1, 2}},
          {Path{xi}, IntSet{6}},
          {Path{yi}, IntSet{10}},
          {Path{yi, xi}, IntSet{11, 12}},
          {Path{ai, xi}, IntSet{13}},
      },
      UpdateKind::Strong);
  EXPECT_EQ(
      tree4,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
          {Path{xi}, IntSet{2}},
          {Path{xi, xi}, IntSet{3, 6}},
          {Path{xi, xi, xi}, IntSet{13}},
          {Path{xi, yi}, IntSet{4, 10}},
          {Path{xi, yi, xi}, IntSet{11, 12, 13}},
          {Path{xi, ai}, IntSet{5}},
          {Path{xi, ai, xi}, IntSet{13}},
          {Path{xi, yi, xi}, IntSet{13}},
          {Path{xi, yi, xi, yi}, IntSet{6}},
          {Path{yi}, IntSet{2, 7}},
          {Path{yi, xi}, IntSet{6, 8}},
          {Path{yi, yi}, IntSet{10}},
          {Path{yi, yi, xi}, IntSet{11, 12, 13}},
          {Path{yi, ai, xi}, IntSet{13}},
          {Path{ai}, IntSet{1, 2}},
          {Path{ai, xi}, IntSet{6}},
          {Path{ai, yi}, IntSet{10}},
          {Path{ai, yi, xi}, IntSet{11, 12}},
          {Path{ai, ai, xi}, IntSet{13}},
      }));
}

TEST_F(AbstractTreeDomainTest, LessOrEqual) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  EXPECT_TRUE(IntSetTree::bottom().leq(IntSetTree::bottom()));
  EXPECT_TRUE(IntSetTree().leq(IntSetTree::bottom()));

  EXPECT_TRUE(IntSetTree::bottom().leq(IntSetTree()));
  EXPECT_TRUE(IntSetTree().leq(IntSetTree()));

  auto tree1 = IntSetTree{IntSet{1}};
  EXPECT_FALSE(tree1.leq(IntSetTree::bottom()));
  EXPECT_FALSE(tree1.leq(IntSetTree{}));
  EXPECT_TRUE(IntSetTree::bottom().leq(tree1));
  EXPECT_TRUE(IntSetTree().leq(tree1));
  EXPECT_TRUE(tree1.leq(tree1));

  auto tree2 = IntSetTree{IntSet{1, 2}};
  EXPECT_FALSE(tree2.leq(IntSetTree::bottom()));
  EXPECT_FALSE(tree2.leq(IntSetTree{}));
  EXPECT_TRUE(IntSetTree::bottom().leq(tree2));
  EXPECT_TRUE(IntSetTree().leq(tree2));
  EXPECT_TRUE(tree1.leq(tree2));
  EXPECT_FALSE(tree2.leq(tree1));
  EXPECT_TRUE(tree2.leq(tree2));

  auto tree3 = IntSetTree{IntSet{2, 3}};
  EXPECT_FALSE(tree1.leq(tree3));
  EXPECT_FALSE(tree2.leq(tree3));
  EXPECT_FALSE(tree3.leq(tree1));
  EXPECT_FALSE(tree3.leq(tree2));

  auto tree4 = IntSetTree{IntSet{1}};
  tree4.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  EXPECT_FALSE(tree4.leq(IntSetTree::bottom()));
  EXPECT_FALSE(tree4.leq(IntSetTree{}));
  EXPECT_TRUE(IntSetTree::bottom().leq(tree4));
  EXPECT_TRUE(IntSetTree().leq(tree4));
  EXPECT_TRUE(tree1.leq(tree4));
  EXPECT_FALSE(tree4.leq(tree1));
  EXPECT_FALSE(tree2.leq(tree4));
  EXPECT_TRUE(tree4.leq(tree2));
  EXPECT_FALSE(tree3.leq(tree4));
  EXPECT_FALSE(tree4.leq(tree3));

  auto tree5 = IntSetTree{IntSet{1}};
  tree5.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree5.write(Path{y}, IntSet{3}, UpdateKind::Weak);
  EXPECT_TRUE(tree1.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree1));
  EXPECT_FALSE(tree2.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree2));
  EXPECT_FALSE(tree3.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree3));
  EXPECT_TRUE(tree4.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree4));

  auto tree6 = IntSetTree{IntSet{1, 2}};
  tree6.write(Path{x, y}, IntSet{3}, UpdateKind::Weak);
  EXPECT_TRUE(tree1.leq(tree6));
  EXPECT_FALSE(tree6.leq(tree1));
  EXPECT_TRUE(tree2.leq(tree6));
  EXPECT_FALSE(tree6.leq(tree2));
  EXPECT_FALSE(tree3.leq(tree6));
  EXPECT_FALSE(tree6.leq(tree3));
  EXPECT_TRUE(tree4.leq(tree6));
  EXPECT_FALSE(tree6.leq(tree4));
  EXPECT_FALSE(tree5.leq(tree6));
  EXPECT_FALSE(tree6.leq(tree5));

  auto tree7 = IntSetTree{IntSet{1}};
  tree7.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree7.write(Path{x, y}, IntSet{3}, UpdateKind::Weak);
  EXPECT_TRUE(tree1.leq(tree7));
  EXPECT_FALSE(tree7.leq(tree1));
  EXPECT_FALSE(tree2.leq(tree7));
  EXPECT_FALSE(tree7.leq(tree2));
  EXPECT_FALSE(tree3.leq(tree7));
  EXPECT_FALSE(tree7.leq(tree3));
  EXPECT_TRUE(tree4.leq(tree7));
  EXPECT_FALSE(tree7.leq(tree4));
  EXPECT_FALSE(tree5.leq(tree7));
  EXPECT_FALSE(tree7.leq(tree5));
  EXPECT_FALSE(tree6.leq(tree7));
  EXPECT_TRUE(tree7.leq(tree6));

  auto tree8 = IntSetTree{IntSet{1, 2, 3}};
  EXPECT_TRUE(tree1.leq(tree8));
  EXPECT_FALSE(tree8.leq(tree1));
  EXPECT_TRUE(tree2.leq(tree8));
  EXPECT_FALSE(tree8.leq(tree2));
  EXPECT_TRUE(tree3.leq(tree8));
  EXPECT_FALSE(tree8.leq(tree3));
  EXPECT_TRUE(tree4.leq(tree8));
  EXPECT_FALSE(tree8.leq(tree4));
  EXPECT_TRUE(tree5.leq(tree8));
  EXPECT_FALSE(tree8.leq(tree5));
  EXPECT_TRUE(tree6.leq(tree8));
  EXPECT_FALSE(tree8.leq(tree6));
  EXPECT_TRUE(tree7.leq(tree8));
  EXPECT_FALSE(tree8.leq(tree7));
}

TEST_F(AbstractTreeDomainTest, LessOrEqualWithIndex) {
  const auto x = PathElement::field("x");
  const auto xi = PathElement::index("x");
  const auto yi = PathElement::index("y");
  const auto ai = PathElement::any_index();

  auto tree1 = IntSetTree{IntSet{1}};
  tree1.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  EXPECT_FALSE(tree1.leq(IntSetTree::bottom()));
  EXPECT_FALSE(tree1.leq(IntSetTree{}));
  EXPECT_TRUE(IntSetTree::bottom().leq(tree1));
  EXPECT_TRUE(IntSetTree().leq(tree1));
  EXPECT_TRUE(tree1.leq(tree1));

  auto tree2 = IntSetTree{IntSet{1}};
  tree2.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree2.write(Path{xi}, IntSet{3}, UpdateKind::Weak);
  EXPECT_TRUE(tree1.leq(tree2));
  EXPECT_FALSE(tree2.leq(tree1));

  auto tree3 = IntSetTree{IntSet{1}};
  tree3.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree3.write(Path{ai}, IntSet{3}, UpdateKind::Weak);
  EXPECT_TRUE(tree1.leq(tree3));
  EXPECT_FALSE(tree3.leq(tree1));
  EXPECT_TRUE(tree2.leq(tree3));
  EXPECT_FALSE(tree3.leq(tree2));

  auto tree4 = IntSetTree{IntSet{1, 2}};
  tree4.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree4.write(Path{xi, ai}, IntSet{3}, UpdateKind::Weak);
  tree4.write(Path{ai, yi}, IntSet{4}, UpdateKind::Weak);
  EXPECT_TRUE(tree1.leq(tree4));
  EXPECT_FALSE(tree4.leq(tree1));
  EXPECT_FALSE(tree2.leq(tree4));
  EXPECT_FALSE(tree4.leq(tree2));
  EXPECT_FALSE(tree3.leq(tree4));
  EXPECT_FALSE(tree4.leq(tree3));

  auto tree5 = IntSetTree{IntSet{1, 2}};
  tree5.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree5.write(Path{ai}, IntSet{3, 4}, UpdateKind::Weak);
  EXPECT_TRUE(tree1.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree1));
  EXPECT_TRUE(tree2.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree2));
  EXPECT_TRUE(tree3.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree3));
  EXPECT_TRUE(tree4.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree4));
}

TEST_F(AbstractTreeDomainTest, Equal) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  EXPECT_TRUE(IntSetTree::bottom().equals(IntSetTree::bottom()));
  EXPECT_TRUE(IntSetTree().equals(IntSetTree::bottom()));
  EXPECT_TRUE(IntSetTree::bottom().equals(IntSetTree()));
  EXPECT_TRUE(IntSetTree().equals(IntSetTree()));

  auto tree1 = IntSetTree{IntSet{1}};
  EXPECT_FALSE(tree1.equals(IntSetTree::bottom()));
  EXPECT_FALSE(IntSetTree::bottom().equals(tree1));
  EXPECT_TRUE(tree1.equals(tree1));

  auto tree2 = IntSetTree{IntSet{1, 2}};
  EXPECT_FALSE(tree2.equals(IntSetTree::bottom()));
  EXPECT_FALSE(IntSetTree::bottom().equals(tree2));
  EXPECT_FALSE(tree1.equals(tree2));
  EXPECT_TRUE(tree2.equals(tree2));

  auto tree3 = IntSetTree{IntSet{2, 3}};
  EXPECT_FALSE(tree1.equals(tree3));
  EXPECT_FALSE(tree2.equals(tree3));
  EXPECT_TRUE(tree3.equals(tree3));

  auto tree4 = IntSetTree{IntSet{1}};
  tree4.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  EXPECT_FALSE(tree4.equals(IntSetTree::bottom()));
  EXPECT_FALSE(IntSetTree::bottom().equals(tree4));
  EXPECT_FALSE(tree1.equals(tree4));
  EXPECT_FALSE(tree2.equals(tree4));
  EXPECT_FALSE(tree3.equals(tree4));
  EXPECT_TRUE(tree4.equals(tree4));

  auto tree5 = IntSetTree{IntSet{1}};
  tree5.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree5.write(Path{y}, IntSet{3}, UpdateKind::Weak);
  EXPECT_FALSE(tree1.equals(tree5));
  EXPECT_FALSE(tree2.equals(tree5));
  EXPECT_FALSE(tree3.equals(tree5));
  EXPECT_FALSE(tree4.equals(tree5));
  EXPECT_TRUE(tree5.equals(tree5));

  auto tree6 = IntSetTree{IntSet{1, 2}};
  tree6.write(Path{x, y}, IntSet{3}, UpdateKind::Weak);
  EXPECT_FALSE(tree1.equals(tree6));
  EXPECT_FALSE(tree2.equals(tree6));
  EXPECT_FALSE(tree3.equals(tree6));
  EXPECT_FALSE(tree4.equals(tree6));
  EXPECT_FALSE(tree5.equals(tree6));
  EXPECT_TRUE(tree6.equals(tree6));

  auto tree7 = IntSetTree{IntSet{1}};
  tree7.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree7.write(Path{x, y}, IntSet{3}, UpdateKind::Weak);
  EXPECT_FALSE(tree1.equals(tree7));
  EXPECT_FALSE(tree2.equals(tree7));
  EXPECT_FALSE(tree3.equals(tree7));
  EXPECT_FALSE(tree4.equals(tree7));
  EXPECT_FALSE(tree5.equals(tree7));
  EXPECT_FALSE(tree6.equals(tree7));
  EXPECT_TRUE(tree7.equals(tree7));

  auto tree8 = IntSetTree{IntSet{1, 2, 3}};
  EXPECT_FALSE(tree1.equals(tree8));
  EXPECT_FALSE(tree2.equals(tree8));
  EXPECT_FALSE(tree3.equals(tree8));
  EXPECT_FALSE(tree4.equals(tree8));
  EXPECT_FALSE(tree5.equals(tree8));
  EXPECT_FALSE(tree6.equals(tree8));
  EXPECT_FALSE(tree7.equals(tree8));
  EXPECT_TRUE(tree8.equals(tree8));

  // Copy of tree 5, with different orders for the successors.
  auto tree9 = IntSetTree{IntSet{1}};
  tree9.write(Path{y}, IntSet{3}, UpdateKind::Weak);
  tree9.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  EXPECT_FALSE(tree1.equals(tree9));
  EXPECT_FALSE(tree2.equals(tree9));
  EXPECT_FALSE(tree3.equals(tree9));
  EXPECT_FALSE(tree4.equals(tree9));
  EXPECT_TRUE(tree5.equals(tree9));
  EXPECT_FALSE(tree6.equals(tree9));
  EXPECT_FALSE(tree7.equals(tree9));
  EXPECT_FALSE(tree8.equals(tree9));
  EXPECT_TRUE(tree9.equals(tree9));
}

TEST_F(AbstractTreeDomainTest, Collapse) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree1 = IntSetTree{IntSet{1}};
  EXPECT_EQ(tree1.collapse(), IntSet{1});

  auto tree2 = IntSetTree{IntSet{1, 2}};
  EXPECT_EQ(tree2.collapse(), (IntSet{1, 2}));

  auto tree4 = IntSetTree{IntSet{1}};
  tree4.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  EXPECT_EQ(tree4.collapse(), (IntSet{1, 2}));

  auto tree5 = IntSetTree{IntSet{1}};
  tree5.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree5.write(Path{y}, IntSet{3}, UpdateKind::Weak);
  EXPECT_EQ(tree5.collapse(), (IntSet{1, 2, 3}));

  auto tree6 = IntSetTree{IntSet{1, 2}};
  tree6.write(Path{x, y}, IntSet{3}, UpdateKind::Weak);
  EXPECT_EQ(tree6.collapse(), (IntSet{1, 2, 3}));

  auto tree7 = IntSetTree{IntSet{1}};
  tree7.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree7.write(Path{x, y}, IntSet{3}, UpdateKind::Weak);
  tree7.write(Path{z, y, x}, IntSet{1, 4}, UpdateKind::Weak);
  EXPECT_EQ(tree7.collapse(), (IntSet{1, 2, 3, 4}));
}

TEST_F(AbstractTreeDomainTest, CollapseWithIndex) {
  const auto x = PathElement::field("x");
  const auto xi = PathElement::index("x");
  const auto ai = PathElement::any_index();

  auto tree1 = IntSetTree{{Path{}, IntSet{1}}, {Path{xi}, IntSet{2}}};
  EXPECT_EQ(tree1.collapse(), (IntSet{1, 2}));

  auto tree2 = IntSetTree{{Path{}, IntSet{1}}, {Path{xi}, IntSet{2}}};
  tree2.write(Path{ai}, IntSet{3}, UpdateKind::Weak);
  tree2.write(Path{x}, IntSet{4}, UpdateKind::Weak);
  EXPECT_EQ(tree2.collapse(), (IntSet{1, 2, 3, 4}));

  auto tree5 = IntSetTree{IntSet{1}};
  tree5.write(Path{xi, ai}, IntSet{2}, UpdateKind::Weak);
  tree5.write(Path{ai, x, xi}, IntSet{3, 4}, UpdateKind::Weak);
  EXPECT_EQ(tree5.collapse(), (IntSet{1, 2, 3, 4}));
}

TEST_F(AbstractTreeDomainTest, CollapseDeeperThan) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetTree{IntSet{1}};
  tree.collapse_deeper_than(1);
  EXPECT_EQ(tree, IntSetTree{IntSet{1}});

  tree = IntSetTree{
      {Path{}, IntSet{1}},
      {Path{x}, IntSet{2}},
      {Path{x, x}, IntSet{3}},
      {Path{x, y}, IntSet{4}},
      {Path{x, z, x}, IntSet{5}},
      {Path{y}, IntSet{10}},
      {Path{y, z}, IntSet{11}},
      {Path{y, z, x}, IntSet{12}},
  };
  tree.collapse_deeper_than(3);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
          {Path{x, x}, IntSet{3}},
          {Path{x, y}, IntSet{4}},
          {Path{x, z, x}, IntSet{5}},
          {Path{y}, IntSet{10}},
          {Path{y, z}, IntSet{11}},
          {Path{y, z, x}, IntSet{12}},
      }));

  tree.collapse_deeper_than(2);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
          {Path{x, x}, IntSet{3}},
          {Path{x, y}, IntSet{4}},
          {Path{x, z}, IntSet{5}},
          {Path{y}, IntSet{10}},
          {Path{y, z}, IntSet{11, 12}},
      }));

  tree.collapse_deeper_than(1);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2, 3, 4, 5}},
          {Path{y}, IntSet{10, 11, 12}},
      }));

  tree.collapse_deeper_than(0);
  EXPECT_EQ(tree, (IntSetTree{IntSet{1, 2, 3, 4, 5, 10, 11, 12}}));
}

TEST_F(AbstractTreeDomainTest, CollapseDeeperThanWithIndex) {
  const auto x = PathElement::field("x");
  const auto xi = PathElement::index("x");
  const auto yi = PathElement::index("y");
  const auto ai = PathElement::any_index();

  auto tree = IntSetTree{
      {Path{}, IntSet{1}},
      {Path{x}, IntSet{2}},
      {Path{xi, xi}, IntSet{3}},
      {Path{xi, yi}, IntSet{4}},
      {Path{xi, ai}, IntSet{5}},
      {Path{xi, yi, xi, yi}, IntSet{6}},
      {Path{yi}, IntSet{7}},
      {Path{yi, xi}, IntSet{8}},
      {Path{x, xi, yi, ai}, IntSet{9}},
  };

  tree.collapse_deeper_than(3);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
          {Path{xi, xi}, IntSet{3}},
          {Path{xi, yi}, IntSet{4}},
          {Path{xi, ai}, IntSet{5}},
          {Path{xi, yi, xi}, IntSet{6}},
          {Path{yi}, IntSet{7}},
          {Path{yi, xi}, IntSet{8}},
          {Path{x, xi, yi}, IntSet{9}},
      }));

  tree.collapse_deeper_than(2);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
          {Path{xi, xi}, IntSet{3}},
          {Path{xi, yi}, IntSet{4, 6}},
          {Path{xi, ai}, IntSet{5}},
          {Path{yi}, IntSet{7}},
          {Path{yi, xi}, IntSet{8}},
          {Path{x, xi}, IntSet{9}},

      }));

  tree.collapse_deeper_than(1);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
          {Path{xi}, IntSet{3, 4, 5, 6}},
          {Path{yi}, IntSet{7, 8}},
          {Path{x}, IntSet{9}},
      }));

  tree.collapse_deeper_than(0);
  EXPECT_EQ(tree, (IntSetTree{IntSet{1, 2, 3, 4, 5, 6, 7, 8, 9}}));
}

TEST_F(AbstractTreeDomainTest, Prune) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree1 = IntSetTree{IntSet{1}};
  tree1.prune(IntSet{1});
  EXPECT_EQ(tree1, IntSetTree{IntSet{}});

  auto tree2 = IntSetTree{IntSet{1, 2}};
  tree2.prune(IntSet{1});
  EXPECT_EQ(tree2, (IntSetTree{IntSet{2}}));

  auto tree4 = IntSetTree{IntSet{1}};
  tree4.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree4.prune(IntSet{2});
  EXPECT_EQ(tree4, (IntSetTree{IntSet{1}}));

  auto tree5 = IntSetTree{IntSet{1}};
  tree5.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree5.write(Path{y}, IntSet{3}, UpdateKind::Weak);
  tree5.prune(IntSet{2, 3});
  EXPECT_EQ(tree5, (IntSetTree{IntSet{1}}));

  auto tree6 = IntSetTree{IntSet{1, 2}};
  tree6.write(Path{x, y}, IntSet{3, 4}, UpdateKind::Weak);
  tree6.prune(IntSet{2, 4});
  EXPECT_EQ(
      tree6,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x, y}, IntSet{3}},
      }));

  auto tree7 = IntSetTree{IntSet{1}};
  tree7.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree7.write(Path{x, y}, IntSet{3}, UpdateKind::Weak);
  tree7.write(Path{z, y, x}, IntSet{4}, UpdateKind::Weak);
  tree7.prune(IntSet{2, 4});
  EXPECT_EQ(
      tree7,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x, y}, IntSet{3}},
      }));
}

TEST_F(AbstractTreeDomainTest, PruneWithIndex) {
  const auto x = PathElement::field("x");
  const auto xi = PathElement::index("x");
  const auto yi = PathElement::index("y");
  const auto ai = PathElement::any_index();

  auto tree = IntSetTree{
      {Path{}, IntSet{1}},
      {Path{x}, IntSet{2}},
      {Path{xi, xi}, IntSet{3}},
      {Path{xi, yi}, IntSet{4}},
      {Path{xi, ai}, IntSet{5}},
      {Path{xi, yi, xi, yi}, IntSet{6}},
      {Path{yi}, IntSet{7}},
      {Path{yi, xi}, IntSet{8}},
      {Path{x, xi, yi, ai}, IntSet{9}},
  };

  // Prune leaves at different heights
  tree.prune(IntSet{5, 9});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
          {Path{xi, xi}, IntSet{3}},
          {Path{xi, yi}, IntSet{4}},
          {Path{xi, ai}, IntSet{}},
          {Path{xi, yi, xi, yi}, IntSet{6}},
          {Path{yi}, IntSet{7}},
          {Path{yi, xi}, IntSet{8}},
      }));

  // prune intermediate nodes
  tree.prune(IntSet{1, 7});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{}},
          {Path{x}, IntSet{2}},
          {Path{xi, xi}, IntSet{3}},
          {Path{xi, yi}, IntSet{4}},
          {Path{xi, yi, xi, yi}, IntSet{6}},
          {Path{yi}, IntSet{}},
          {Path{yi, xi}, IntSet{8}},
      }));

  tree.prune(IntSet{3, 4, 6, 8});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{}},
          {Path{x}, IntSet{2}},
      }));
}

TEST_F(AbstractTreeDomainTest, DepthExceedingMaxLeaves) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetTree{IntSet{1}};
  EXPECT_EQ(tree.depth_exceeding_max_leaves(2), std::nullopt);

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
  };
  EXPECT_EQ(tree.depth_exceeding_max_leaves(2), std::nullopt);

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
      {Path{y}, IntSet{2}},
  };
  EXPECT_EQ(tree.depth_exceeding_max_leaves(2), std::nullopt);

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
      {Path{y}, IntSet{2}},
      {Path{z}, IntSet{3}},
  };
  EXPECT_EQ(tree.depth_exceeding_max_leaves(2), 0);

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
      {Path{y}, IntSet{2}},
      {Path{z, x}, IntSet{3}},
  };
  EXPECT_EQ(tree.depth_exceeding_max_leaves(2), 0);

  tree = IntSetTree{
      {Path{x, y}, IntSet{1}},
      {Path{y, z}, IntSet{2}},
      {Path{z, x}, IntSet{3}},
  };
  EXPECT_EQ(tree.depth_exceeding_max_leaves(2), 0);

  tree = IntSetTree{
      {Path{x, x}, IntSet{1}},
      {Path{x, y, z}, IntSet{2}},
      {Path{y, z}, IntSet{3}},
  };
  EXPECT_EQ(tree.depth_exceeding_max_leaves(2), 1);

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
      {Path{x, y}, IntSet{2}},
      {Path{x, y, z}, IntSet{3}},
  };
  EXPECT_EQ(tree.depth_exceeding_max_leaves(2), std::nullopt);

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
      {Path{y, z}, IntSet{2}},
      {Path{z, x, y}, IntSet{3}},
  };
  EXPECT_EQ(tree.depth_exceeding_max_leaves(3), std::nullopt);

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
      {Path{x, y}, IntSet{2}},
      {Path{x, y, x}, IntSet{3}},
      {Path{x, y, y}, IntSet{4}},
      {Path{y}, IntSet{5}},
      {Path{z, x, y, z}, IntSet{6}},
  };
  EXPECT_EQ(tree.depth_exceeding_max_leaves(3), 2);
}

TEST_F(AbstractTreeDomainTest, LimitLeaves) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetTree{IntSet{1}};
  tree.limit_leaves(2);
  EXPECT_EQ(tree, IntSetTree{IntSet{1}});

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
  };
  tree.limit_leaves(2);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{x}, IntSet{1}},
      }));

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
      {Path{y}, IntSet{2}},
  };
  tree.limit_leaves(2);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{x}, IntSet{1}},
          {Path{y}, IntSet{2}},
      }));

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
      {Path{y}, IntSet{2}},
      {Path{z}, IntSet{3}},
  };
  tree.limit_leaves(2);
  EXPECT_EQ(tree, (IntSetTree{IntSet{1, 2, 3}}));

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
      {Path{y}, IntSet{2}},
      {Path{z, x}, IntSet{3}},
  };
  tree.limit_leaves(2);
  EXPECT_EQ(tree, (IntSetTree{IntSet{1, 2, 3}}));

  tree = IntSetTree{
      {Path{x, y}, IntSet{1}},
      {Path{y, z}, IntSet{2}},
      {Path{z, x}, IntSet{3}},
  };
  tree.limit_leaves(2);
  EXPECT_EQ(tree, (IntSetTree{IntSet{1, 2, 3}}));

  tree = IntSetTree{
      {Path{x, x}, IntSet{1}},
      {Path{x, y}, IntSet{2}},
      {Path{x, z}, IntSet{3}},
  };
  tree.limit_leaves(2);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{x}, IntSet{1, 2, 3}},
      }));

  tree = IntSetTree{
      {Path{x, x}, IntSet{1}},
      {Path{x, y, z}, IntSet{2}},
      {Path{y, z}, IntSet{3}},
  };
  tree.limit_leaves(2);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{x}, IntSet{1, 2}},
          {Path{y}, IntSet{3}},
      }));

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
      {Path{x, y}, IntSet{2}},
      {Path{x, y, z}, IntSet{3}},
  };
  tree.limit_leaves(2);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{x}, IntSet{1}},
          {Path{x, y}, IntSet{2}},
          {Path{x, y, z}, IntSet{3}},
      }));

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
      {Path{y, z}, IntSet{2}},
      {Path{z, x, y}, IntSet{3}},
  };
  tree.limit_leaves(3);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{x}, IntSet{1}},
          {Path{y, z}, IntSet{2}},
          {Path{z, x, y}, IntSet{3}},
      }));

  tree = IntSetTree{
      {Path{x}, IntSet{1}},
      {Path{x, y}, IntSet{2}},
      {Path{x, y, x}, IntSet{3}},
      {Path{x, y, y}, IntSet{4}},
      {Path{y}, IntSet{5}},
      {Path{z, x, y, z}, IntSet{6}},
  };
  tree.limit_leaves(3);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{x}, IntSet{1}},
          {Path{x, y}, IntSet{2, 3, 4}},
          {Path{y}, IntSet{5}},
          {Path{z, x}, IntSet{6}},
      }));
}

TEST_F(AbstractTreeDomainTest, LimitLeavesWithIndex) {
  const auto x = PathElement::field("x");
  const auto xi = PathElement::index("x");
  const auto yi = PathElement::index("y");
  const auto ai = PathElement::any_index();

  // Has 6 leaves
  auto tree = IntSetTree{
      {Path{}, IntSet{1}},
      {Path{x}, IntSet{2}}, // leaf
      {Path{xi, yi, xi}, IntSet{3}},
      {Path{xi, yi, xi, yi}, IntSet{4}}, // leaf
      {Path{xi, yi, yi}, IntSet{5}}, // leaf
      {Path{xi, yi, ai}, IntSet{6}}, // leaf
      {Path{xi, xi, yi, ai}, IntSet{7}}, // leaf
      {Path{yi}, IntSet{8}},
      {Path{yi, xi}, IntSet{9}}, // leaf
  };

  tree.limit_leaves(6);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}}, // leaf
          {Path{xi, yi, xi}, IntSet{3}},
          {Path{xi, yi, xi, yi}, IntSet{4}}, // leaf
          {Path{xi, yi, yi}, IntSet{5}}, // leaf
          {Path{xi, yi, ai}, IntSet{6}}, // leaf
          {Path{xi, xi, yi, ai}, IntSet{7}}, // leaf
          {Path{yi}, IntSet{8}},
          {Path{yi, xi}, IntSet{9}}, // leaf
      }));

  tree.limit_leaves(5);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}}, // leaf
          {Path{xi, yi}, IntSet{3, 4, 5, 6}}, // leaf
          {Path{xi, xi}, IntSet{7}}, // leaf
          {Path{yi}, IntSet{8}},
          {Path{yi, xi}, IntSet{9}}, // leaf
      }));

  tree.limit_leaves(4);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}}, // leaf
          {Path{xi, yi}, IntSet{3, 4, 5, 6}}, // leaf
          {Path{xi, xi}, IntSet{7}}, // leaf
          {Path{yi}, IntSet{8}},
          {Path{yi, xi}, IntSet{9}}, // leaf
      }));

  tree.limit_leaves(3);
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}}, // leaf
          {Path{xi}, IntSet{3, 4, 5, 6, 7}}, // leaf
          {Path{yi}, IntSet{8, 9}}, // leaf
      }));

  tree.limit_leaves(2);
  EXPECT_EQ(tree, (IntSetTree{IntSet{1, 2, 3, 4, 5, 6, 7, 8, 9}}));
}

TEST_F(AbstractTreeDomainTest, Join) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetTree::bottom();
  tree.join_with(IntSetTree{IntSet{1}});
  EXPECT_EQ(tree, (IntSetTree{IntSet{1}}));

  tree.join_with(IntSetTree::bottom());
  EXPECT_EQ(tree, (IntSetTree{IntSet{1}}));

  tree.join_with(IntSetTree{IntSet{2}});
  EXPECT_EQ(tree, (IntSetTree{IntSet{1, 2}}));

  tree.join_with(IntSetTree{{Path{x}, IntSet{1}}});
  EXPECT_EQ(tree, (IntSetTree{IntSet{1, 2}}));

  tree.join_with(IntSetTree{{Path{x}, IntSet{3}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1, 2}},
          {Path{x}, IntSet{3}},
      }));

  tree.join_with(IntSetTree{{Path{x, y}, IntSet{2, 3}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1, 2}},
          {Path{x}, IntSet{3}},
      }));

  tree.join_with(IntSetTree{IntSet{3}});
  EXPECT_EQ(tree, (IntSetTree{IntSet{1, 2, 3}}));

  tree.join_with(IntSetTree{
      {Path{x}, IntSet{4}},
      {Path{x, y}, IntSet{5, 6}},
      {Path{x, z}, IntSet{7, 8}},
      {Path{y}, IntSet{9, 10}},
  });
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1, 2, 3}},
          {Path{x}, IntSet{4}},
          {Path{x, y}, IntSet{5, 6}},
          {Path{x, z}, IntSet{7, 8}},
          {Path{y}, IntSet{9, 10}},
      }));

  tree.join_with(IntSetTree{
      {Path{x}, IntSet{5, 6, 7}},
      {Path{y}, IntSet{10, 11}},
  });
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1, 2, 3}},
          {Path{x}, IntSet{4, 5, 6, 7}},
          {Path{x, z}, IntSet{8}},
          {Path{y}, IntSet{9, 10, 11}},
      }));
}

TEST_F(AbstractTreeDomainTest, JoinIndex) {
  const auto x = PathElement::field("x");
  const auto xi = PathElement::index("x");
  const auto yi = PathElement::index("y");
  const auto zi = PathElement::index("z");
  const auto ai = PathElement::any_index();

  // Expect tree: R: _|_
  auto tree = IntSetTree::bottom();

  // Expect tree: R: {1}
  tree.join_with(IntSetTree{IntSet{1}});
  EXPECT_EQ(tree, (IntSetTree{IntSet{1}}));

  // Expect tree: R: {1}
  tree.join_with(IntSetTree{{Path{x}, IntSet{1}}});
  EXPECT_EQ(tree, (IntSetTree{IntSet{1}}));

  // Expect tree: R: {1}
  //              |- .x: {2}
  tree.join_with(IntSetTree{{Path{x}, IntSet{2}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
      }));

  // Expect tree: R: {1}
  //              |- .x: {2}
  tree.join_with(IntSetTree{{Path{xi}, IntSet{1}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
      }));

  // Expect tree: R: {1}
  //              |- .x: {2}
  //              |- [x]: {2}
  tree.join_with(IntSetTree{{Path{xi}, IntSet{2}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
          {Path{xi}, IntSet{2}},
      }));

  // Expect tree: R: {1}
  //              |- .x: {2, 3}
  //              |- [x]: {2}
  tree.join_with(IntSetTree{{Path{x}, IntSet{3}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2, 3}},
          {Path{xi}, IntSet{2}},
      }));

  // left[*] = _|_
  // right[*] = {1} (existing in left R)
  // Expect tree: R: {1}
  //              |- .x: {2, 3}
  //              |- [x]: {2}
  tree.join_with(IntSetTree{{Path{ai}, IntSet{1}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2, 3}},
          {Path{xi}, IntSet{2}},
      }));

  // left[*] = _|_
  // right[*] = {4}
  // Expect tree: R: {1}
  //              |- .x: {2, 3}
  //              |- [x]: {2, 4}
  //              |- [*]: {4}
  tree.join_with(IntSetTree{{Path{ai}, IntSet{4}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2, 3}},
          {Path{xi}, IntSet{2, 4}},
          {Path{ai}, IntSet{4}},
      }));

  // left[*] = {4}
  // right[*] = {5}
  // Expect tree: R: {1}
  //              |- .x: {2, 3}
  //              |- [x]: {2, 4, 5}
  //              |- [*]: {4, 5}
  tree.join_with(IntSetTree{{Path{ai}, IntSet{5}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2, 3}},
          {Path{xi}, IntSet{2, 4, 5}},
          {Path{ai}, IntSet{4, 5}},
      }));

  // right[y] = {1} (existing in left R)
  // Expect tree: R: {1}
  //              |- .x: {2, 3}
  //              |- [x]: {2, 4, 5}
  //              |- [y]: left[*] /\ right[y] = {1, 4, 5}.
  //                      {1} already in left R, so {4, 5}
  //              |- [*]: {4, 5}
  tree.join_with(IntSetTree{{Path{yi}, IntSet{1}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2, 3}},
          {Path{xi}, IntSet{2, 4, 5}},
          {Path{yi}, IntSet{4, 5}},
          {Path{ai}, IntSet{4, 5}},
      }));

  // left[y] = {4, 5}
  // left[*] = {4, 5}
  // right[y] = {6}
  // right[z] = {7}
  // right[*] = {8}
  // Expect tree: R: {1}
  //              |- .x: {2, 3}
  //              |- [x]: {2, 4, 5, 8}
  //              |- [y]: {4, 5, 6}
  //              |- [z]: {4, 5, 7}
  //              |- [*]: {4, 5, 8}
  tree.join_with(IntSetTree{
      {Path{yi}, IntSet{6}}, {Path{zi}, IntSet{7}}, {Path{ai}, IntSet{8}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2, 3}},
          {Path{xi}, IntSet{2, 4, 5, 8}},
          {Path{yi}, IntSet{4, 5, 6}},
          {Path{zi}, IntSet{4, 5, 7}},
          {Path{ai}, IntSet{4, 5, 8}},
      }));

  // right R = {2, 3, 4, 5}
  // right[x] = {9}
  // Expect tree: R: {1, 2, 3, 4, 5}
  //              |- [x]: {8, 9}
  //              |- [y]: {6}
  //              |- [z]: {7}
  //              |- [*]: {8}
  tree.join_with(
      IntSetTree{{Path{}, IntSet{2, 3, 4, 5}}, {Path{xi}, IntSet{9}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1, 2, 3, 4, 5}},
          {Path{xi}, IntSet{8, 9}},
          {Path{yi}, IntSet{6}},
          {Path{zi}, IntSet{7}},
          {Path{ai}, IntSet{8}},
      }));

  // right.x = {6}
  // right.x[x] = {6, 7}
  // right[y].x = {7}
  // right[z][*] = {8}
  // Expect tree: R: {1, 2, 3, 4, 5}
  //              |- .x: {6}
  //                  |- [x]: {7}
  //              |- [x]: {8, 9}
  //              |- [y]: {6}
  //                  |- .x: {7}
  //              |- [z]: {7}
  //                  |- [*]: {8}
  //              |- [*]: {8}
  tree.join_with(IntSetTree{
      {Path{x}, IntSet{6}},
      {Path{x, xi}, IntSet{6, 7}},
      {Path{yi, x}, IntSet{7}},
      {Path{zi, ai}, IntSet{8}},
  });
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1, 2, 3, 4, 5}},
          {Path{x}, IntSet{6}},
          {Path{x, xi}, IntSet{7}},
          {Path{xi}, IntSet{8, 9}},
          {Path{yi}, IntSet{6}},
          {Path{yi, x}, IntSet{7}},
          {Path{zi}, IntSet{7}},
          {Path{zi, ai}, IntSet{8}},
          {Path{ai}, IntSet{8}},
      }));
}

TEST_F(AbstractTreeDomainTest, Widen) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetTree::bottom();
  tree.widen_with(IntSetTree{IntSet{1}});
  EXPECT_EQ(tree, (IntSetTree{IntSet{1}}));

  tree.widen_with(IntSetTree::bottom());
  EXPECT_EQ(tree, (IntSetTree{IntSet{1}}));

  tree.widen_with(IntSetTree{IntSet{2}});
  EXPECT_EQ(tree, (IntSetTree{IntSet{1, 2}}));

  tree.widen_with(IntSetTree{{Path{x}, IntSet{1}}});
  EXPECT_EQ(tree, (IntSetTree{IntSet{1, 2}}));

  tree.widen_with(IntSetTree{{Path{x}, IntSet{3}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1, 2}},
          {Path{x}, IntSet{3}},
      }));

  tree.widen_with(IntSetTree{{Path{x, y}, IntSet{2, 3}}});
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1, 2}},
          {Path{x}, IntSet{3}},
      }));

  tree.widen_with(IntSetTree{IntSet{3}});
  EXPECT_EQ(tree, (IntSetTree{IntSet{1, 2, 3}}));

  tree.widen_with(IntSetTree{
      {Path{x}, IntSet{4}},
      {Path{x, y}, IntSet{5, 6}},
      {Path{x, z}, IntSet{7, 8}},
      {Path{y}, IntSet{9, 10}},
  });
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1, 2, 3}},
          {Path{x}, IntSet{4}},
          {Path{x, y}, IntSet{5, 6}},
          {Path{x, z}, IntSet{7, 8}},
          {Path{y}, IntSet{9, 10}},
      }));

  tree.widen_with(IntSetTree{
      {Path{x}, IntSet{5, 6, 7}},
      {Path{y}, IntSet{10, 11}},
  });
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1, 2, 3}},
          {Path{x}, IntSet{4, 5, 6, 7}},
          {Path{x, z}, IntSet{8}},
          {Path{y}, IntSet{9, 10, 11}},
      }));

  // Check that we collapse at height 4.
  tree = IntSetTree{
      {Path{}, IntSet{1}},
      {Path{x, y, z, x}, IntSet{2}},
      {Path{x, y, z, x, y}, IntSet{3}},
  };
  tree.widen_with(IntSetTree{
      {Path{}, IntSet{10}},
      {Path{x, y, z, x, z}, IntSet{1, 4}},
  });
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1, 10}},
          {Path{x, y, z, x}, IntSet{2, 3, 4}},
      }));
}

TEST_F(AbstractTreeDomainTest, Read) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetTree{
      {Path{}, IntSet{1}},
      {Path{x}, IntSet{2}},
      {Path{x, z}, IntSet{3}},
      {Path{y}, IntSet{4}},
  };

  EXPECT_EQ(tree.read(Path{}), tree);

  EXPECT_EQ(
      tree.read(Path{x}),
      (IntSetTree{
          {Path{}, IntSet{1, 2}},
          {Path{z}, IntSet{3}},
      }));

  EXPECT_EQ(tree.read(Path{x, z}), (IntSetTree{IntSet{1, 2, 3}}));

  EXPECT_EQ(tree.read(Path{y}), (IntSetTree{IntSet{1, 4}}));

  // Inexisting path returns the join of all ancestors.
  EXPECT_EQ(tree.read(Path{x, z, y}), (IntSetTree{IntSet{1, 2, 3}}));
}

TEST_F(AbstractTreeDomainTest, ReadWithIndex) {
  const auto x = PathElement::field("x");
  const auto xi = PathElement::index("x");
  const auto yi = PathElement::index("y");
  const auto ai = PathElement::any_index();

  auto tree = IntSetTree{
      {Path{}, IntSet{1}},
      {Path{x}, IntSet{2}},
      {Path{xi}, IntSet{3}},
      {Path{xi, yi}, IntSet{4}},
      {Path{yi}, IntSet{5}},
      {Path{ai}, IntSet{6}}, // This will be joined into all existing indices
  };

  EXPECT_EQ(tree.read(Path{}), tree);

  EXPECT_EQ(
      tree.read(Path{x}),
      (IntSetTree{
          {Path{}, IntSet{1, 2}},
      }));

  EXPECT_EQ(
      tree.read(Path{xi}),
      (IntSetTree{
          {Path{}, IntSet{1, 3, 6}},
          {Path{yi}, IntSet{4}},
      }));

  EXPECT_EQ(tree.read(Path{xi, yi}), (IntSetTree{IntSet{1, 3, 4, 6}}));

  EXPECT_EQ(tree.read(Path{yi}), (IntSetTree{IntSet{1, 5, 6}}));

  // Read on [*] includes reads from all sibling indices
  EXPECT_EQ(
      tree.read(Path{ai}),
      (IntSetTree{
          {Path{}, IntSet{1, 3, 5, 6}},
          {Path{yi}, IntSet{4}},
      }));
}

TEST_F(AbstractTreeDomainTest, RawRead) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetTree{
      {Path{}, IntSet{1}},
      {Path{x}, IntSet{2}},
      {Path{x, z}, IntSet{3}},
      {Path{y}, IntSet{4}},
  };

  EXPECT_EQ(tree.raw_read(Path{}), tree);

  EXPECT_EQ(
      tree.raw_read(Path{x}),
      (IntSetTree{
          {Path{}, IntSet{2}},
          {Path{z}, IntSet{3}},
      }));

  EXPECT_EQ(tree.raw_read(Path{x, z}), (IntSetTree{IntSet{3}}));

  EXPECT_EQ(tree.raw_read(Path{y}), (IntSetTree{IntSet{4}}));

  EXPECT_EQ(tree.raw_read(Path{z}), IntSetTree::bottom());

  EXPECT_EQ(tree.raw_read(Path{x, y}), IntSetTree::bottom());
}

TEST_F(AbstractTreeDomainTest, Elements) {
  using Pair = std::pair<Path, IntSet>;

  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetTree{
      {Path{x}, IntSet{1, 2}},
      {Path{x, y}, IntSet{3, 4}},
      {Path{x, z}, IntSet{5, 6}},
      {Path{x, z, y}, IntSet{7, 8}},
      {Path{x, x}, IntSet{9, 10}},
  };
  EXPECT_THAT(
      tree.elements(),
      testing::UnorderedElementsAre(
          Pair{Path{x}, IntSet{1, 2}},
          Pair{Path{x, y}, IntSet{3, 4}},
          Pair{Path{x, z}, IntSet{5, 6}},
          Pair{Path{x, z, y}, IntSet{7, 8}},
          Pair{Path{x, x}, IntSet{9, 10}}));
}

TEST_F(AbstractTreeDomainTest, Map) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  auto tree = IntSetTree{
      {Path{}, IntSet{1, 2}},
      {Path{x}, IntSet{3, 4}},
      {Path{x, y}, IntSet{5, 6}},
      {Path{y}, IntSet{7, 8}},
      {Path{y, x}, IntSet{9, 10}},
  };
  tree.map([](IntSet& set) {
    auto copy = set;
    set = IntSet{};
    for (int value : copy.elements()) {
      set.add(value * value);
    }
  });
  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1, 4}},
          {Path{x}, IntSet{9, 16}},
          {Path{x, y}, IntSet{25, 36}},
          {Path{y}, IntSet{49, 64}},
          {Path{y, x}, IntSet{81, 100}},
      }));
}

namespace {

struct PropagateArtificialSources {
  Taint operator()(Taint taint, Path::Element path_element) const {
    taint.append_to_artificial_source_input_paths(path_element);
    return taint;
  }
};

} // namespace

TEST_F(AbstractTreeDomainTest, Propagate) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = TaintTree{make_artificial_source(1)};
  EXPECT_EQ(
      tree.read(Path{x, y}, PropagateArtificialSources()),
      (TaintTree{
          Taint::artificial_source(
              AccessPath(Root(Root::Kind::Argument, 1), Path{x, y})),
      }));

  tree.write(Path{x}, make_artificial_source(2), UpdateKind::Weak);
  EXPECT_EQ(
      tree.read(Path{x, y}, PropagateArtificialSources()),
      (TaintTree{
          Taint::artificial_source(
              AccessPath(Root(Root::Kind::Argument, 1), Path{x, y}))
              .join(Taint::artificial_source(
                  AccessPath(Root(Root::Kind::Argument, 2), Path{y}))),
      }));

  tree.write(Path{x, y}, make_artificial_source(3), UpdateKind::Weak);
  EXPECT_EQ(
      tree.read(Path{x, y}, PropagateArtificialSources()),
      (TaintTree{
          Taint::artificial_source(
              AccessPath(Root(Root::Kind::Argument, 1), Path{x, y}))
              .join(Taint::artificial_source(
                  AccessPath(Root(Root::Kind::Argument, 2), Path{y})))
              .join(Taint::artificial_source(
                  AccessPath(Root(Root::Kind::Argument, 3)))),
      }));

  tree.write(Path{x, y, z}, make_artificial_source(4), UpdateKind::Weak);
  EXPECT_EQ(
      tree.read(Path{x, y}, PropagateArtificialSources()),
      (TaintTree{
          {
              Path{},
              Taint::artificial_source(
                  AccessPath(Root(Root::Kind::Argument, 1), Path{x, y})),
          },
          {
              Path{},
              Taint::artificial_source(
                  AccessPath(Root(Root::Kind::Argument, 2), Path{y})),
          },
          {
              Path{},
              Taint::artificial_source(
                  AccessPath(Root(Root::Kind::Argument, 3))),
          },
          {
              Path{z},
              Taint::artificial_source(
                  AccessPath(Root(Root::Kind::Argument, 4))),
          },
      }));

  tree = TaintTree{
      Taint::artificial_source(
          AccessPath(Root(Root::Kind::Argument, 0), Path{x})),
  };
  EXPECT_EQ(
      tree.read(Path{y}, PropagateArtificialSources()),
      (TaintTree{
          Taint::artificial_source(
              AccessPath(Root(Root::Kind::Argument, 0), Path{x, y})),
      }));

  tree.set_to_bottom();
  tree.write(Path{x}, make_artificial_source(0), UpdateKind::Weak);
  tree.write(Path{y}, make_artificial_source(1), UpdateKind::Weak);
  tree.write(Path{z}, make_artificial_source(2), UpdateKind::Weak);
  tree.write(
      Path{y, z},
      Taint::artificial_source(
          AccessPath(Root(Root::Kind::Argument, 1), Path{z})),
      UpdateKind::Weak);
  EXPECT_EQ(
      tree.read(Path{y, z}, PropagateArtificialSources()),
      (TaintTree{
          Taint::artificial_source(
              AccessPath(Root(Root::Kind::Argument, 1), Path{z})),
      }));
}

TEST_F(AbstractTreeDomainTest, Transform) {
  const Feature broadening = Feature("via-broadening");
  const FeatureMayAlwaysSet features = FeatureMayAlwaysSet({&broadening});
  const auto& transform = [&features](Taint& taint) {
    taint.add_inferred_features(features);
    return taint;
  };

  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  // Test collapse
  auto tree = TaintTree{make_artificial_source(1)};
  EXPECT_EQ(
      tree.collapse(transform),
      Taint::artificial_source(AccessPath(Root(Root::Kind::Argument, 1))));

  tree.write(Path{x}, make_artificial_source(2), UpdateKind::Weak);
  EXPECT_EQ(
      tree.collapse(transform),
      Taint(
          {test::make_taint_config(
               Kinds::artificial_source(),
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                   .input_paths =
                       PathTreeDomain{{Path{}, SingletonAbstractDomain()}}}),
           test::make_taint_config(
               Kinds::artificial_source(),
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                   .locally_inferred_features = features,
                   .input_paths =
                       PathTreeDomain{{Path{}, SingletonAbstractDomain()}}})}));

  tree.write(Path{x, y}, make_artificial_source(3), UpdateKind::Weak);
  EXPECT_EQ(
      tree.collapse(transform),
      Taint(
          {test::make_taint_config(
               Kinds::artificial_source(),
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                   .input_paths =
                       PathTreeDomain{{Path{}, SingletonAbstractDomain()}}}),
           test::make_taint_config(
               Kinds::artificial_source(),
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                   .locally_inferred_features = features,
                   .input_paths =
                       PathTreeDomain{{Path{}, SingletonAbstractDomain()}}}),
           test::make_taint_config(
               Kinds::artificial_source(),
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 3)),
                   .locally_inferred_features = features,
                   .input_paths =
                       PathTreeDomain{{Path{}, SingletonAbstractDomain()}}})}));

  tree.write(Path{}, make_artificial_source(3), UpdateKind::Weak);
  EXPECT_EQ(
      tree.collapse(transform),
      Taint(
          {test::make_taint_config(
               Kinds::artificial_source(),
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                   .input_paths =
                       PathTreeDomain{{Path{}, SingletonAbstractDomain()}}}),
           test::make_taint_config(
               Kinds::artificial_source(),
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                   .locally_inferred_features = features,
                   .input_paths =
                       PathTreeDomain{{Path{}, SingletonAbstractDomain()}}}),
           test::make_taint_config(
               Kinds::artificial_source(),
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 3)),
                   .input_paths =
                       PathTreeDomain{{Path{}, SingletonAbstractDomain()}}})}));

  // Test collapse_inplace
  auto tree2 = TaintTree{make_artificial_source(1)};
  tree2.collapse_inplace(transform);
  EXPECT_EQ(tree2, TaintTree{make_artificial_source(1)});

  tree2.write(Path{x}, make_artificial_source(2), UpdateKind::Weak);
  tree2.write(Path{x, y}, make_artificial_source(3), UpdateKind::Weak);
  tree2.collapse_inplace(transform);
  EXPECT_EQ(
      tree2,
      (TaintTree{
          {
              Path{},
              Taint::artificial_source(
                  AccessPath(Root(Root::Kind::Argument, 1))),
          },
          {
              Path{},
              Taint{test::make_taint_config(
                  Kinds::artificial_source(),
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                      .locally_inferred_features = features,
                      .input_paths =
                          PathTreeDomain{
                              {Path{}, SingletonAbstractDomain()}}})},
          },
          {
              Path{},
              Taint{test::make_taint_config(
                  Kinds::artificial_source(),
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 3)),
                      .locally_inferred_features = features,
                      .input_paths =
                          PathTreeDomain{
                              {Path{}, SingletonAbstractDomain()}}})},
          }}));

  // Test limit_leaves
  auto tree3 = TaintTree{make_artificial_source(1)};
  tree3.write(Path{x}, TaintTree{make_artificial_source(2)}, UpdateKind::Weak);
  tree3.write(Path{y}, TaintTree{make_artificial_source(3)}, UpdateKind::Weak);
  tree3.write(Path{z}, TaintTree{make_artificial_source(4)}, UpdateKind::Weak);
  tree3.limit_leaves(2, transform);
  EXPECT_EQ(
      tree3,
      (TaintTree{
          {
              Path{},
              Taint::artificial_source(
                  AccessPath(Root(Root::Kind::Argument, 1))),
          },
          {
              Path{},
              Taint{test::make_taint_config(
                  Kinds::artificial_source(),
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                      .locally_inferred_features = features,
                      .input_paths =
                          PathTreeDomain{
                              {Path{}, SingletonAbstractDomain()}}})},
          },
          {
              Path{},
              Taint{test::make_taint_config(
                  Kinds::artificial_source(),
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 3)),
                      .locally_inferred_features = features,
                      .input_paths =
                          PathTreeDomain{
                              {Path{}, SingletonAbstractDomain()}}})},
          },
          {
              Path{},
              Taint{test::make_taint_config(
                  Kinds::artificial_source(),
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 4)),
                      .locally_inferred_features = features,
                      .input_paths =
                          PathTreeDomain{
                              {Path{}, SingletonAbstractDomain()}}})},
          }}));
}

TEST_F(AbstractTreeDomainTest, CollapseInvalid) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetTree{IntSet{1}};
  tree.write(Path{x}, IntSet{2}, UpdateKind::Weak);
  tree.write(Path{x, z}, IntSet{3}, UpdateKind::Weak);
  tree.write(Path{x, y}, IntSet{4}, UpdateKind::Weak);
  tree.write(Path{y}, IntSet{5}, UpdateKind::Weak);
  tree.write(Path{z}, IntSet{6}, UpdateKind::Weak);

  using Accumulator = Path;

  // Invalid paths are z and x.y (x.z is valid)
  auto is_valid =
      [x, y, z](const Accumulator& previous_path, Path::Element path_element) {
        if ((previous_path == Path() && path_element == z) ||
            (previous_path == Path{x} && path_element == y)) {
          return std::make_pair(false, Path());
        }

        auto current_path = previous_path;
        current_path.append(path_element);
        return std::make_pair(true, current_path);
      };
  tree.collapse_invalid_paths<Accumulator>(is_valid, Path());

  EXPECT_EQ(
      tree,
      (IntSetTree{
          {Path{}, IntSet{1, 6}}, // originally {} and z
          {Path{x}, IntSet{2, 4}}, // originally x and x.y
          {Path{x, z}, IntSet{3}},
          {Path{y}, IntSet{5}},
      }));
}

} // namespace marianatrench
