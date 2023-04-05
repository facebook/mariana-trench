/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <PatriciaTreeSetAbstractDomain.h>

#include <mariana-trench/AccessPathTreeDomain.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class AccessPathTreeDomainTest : public test::Test {};

using IntSet = sparta::PatriciaTreeSetAbstractDomain<unsigned>;
using IntSetPathTree = AbstractTreeDomain<IntSet>;
using IntSetAccessPathTree = AccessPathTreeDomain<IntSet>;

TEST_F(AccessPathTreeDomainTest, DefaultConstructor) {
  EXPECT_TRUE(IntSetAccessPathTree().is_bottom());
}

TEST_F(AccessPathTreeDomainTest, WriteWeak) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  auto tree = IntSetAccessPathTree();
  EXPECT_TRUE(tree.is_bottom());

  tree.write(AccessPath(Root(Root::Kind::Return)), IntSet{1}, UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.read(Root(Root::Kind::Return)), (IntSetPathTree{IntSet{1}}));
  EXPECT_TRUE(tree.read(Root(Root::Kind::Argument, 0)).is_bottom());

  tree.write(
      AccessPath(Root(Root::Kind::Return), Path{x}),
      IntSet{1, 2},
      UpdateKind::Weak);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)),
      (IntSetPathTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
      }));
  EXPECT_TRUE(tree.read(Root(Root::Kind::Argument, 0)).is_bottom());

  tree.write(
      AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
      IntSet{3},
      UpdateKind::Weak);
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)),
      (IntSetPathTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 0)),
      (IntSetPathTree{
          {Path{y}, IntSet{3}},
      }));

  tree.write(
      AccessPath(Root(Root::Kind::Argument, 1)), IntSet{1}, UpdateKind::Weak);
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)),
      (IntSetPathTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 0)),
      (IntSetPathTree{
          {Path{y}, IntSet{3}},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 1)), (IntSetPathTree{IntSet{1}}));
}

TEST_F(AccessPathTreeDomainTest, WriteStrong) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  auto tree = IntSetAccessPathTree();
  EXPECT_TRUE(tree.is_bottom());

  tree.write(
      AccessPath(Root(Root::Kind::Return)), IntSet{1}, UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(tree.read(Root(Root::Kind::Return)), (IntSetPathTree{IntSet{1}}));
  EXPECT_TRUE(tree.read(Root(Root::Kind::Argument, 0)).is_bottom());

  tree.write(
      AccessPath(Root(Root::Kind::Return), Path{x}),
      IntSet{1, 2},
      UpdateKind::Strong);
  EXPECT_FALSE(tree.is_bottom());
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)),
      (IntSetPathTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
      }));
  EXPECT_TRUE(tree.read(Root(Root::Kind::Argument, 0)).is_bottom());

  tree.write(
      AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
      IntSet{3},
      UpdateKind::Strong);
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)),
      (IntSetPathTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 0)),
      (IntSetPathTree{
          {Path{y}, IntSet{3}},
      }));

  tree.write(
      AccessPath(Root(Root::Kind::Argument, 1)), IntSet{1}, UpdateKind::Strong);
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)),
      (IntSetPathTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 0)),
      (IntSetPathTree{
          {Path{y}, IntSet{3}},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 1)), (IntSetPathTree{IntSet{1}}));
}

TEST_F(AccessPathTreeDomainTest, Read) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  auto tree = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1}},
      {AccessPath(Root(Root::Kind::Return), Path{x}), IntSet{2}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{y}), IntSet{3}},
      {AccessPath(Root(Root::Kind::Argument, 1)), IntSet{4}},
  };
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Return))),
      (IntSetPathTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
      }));
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Return), Path{x})),
      (IntSetPathTree{IntSet{1, 2}}));
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Return), Path{x, y})),
      (IntSetPathTree{IntSet{1, 2}}));
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Return), Path{y})),
      (IntSetPathTree{IntSet{1}}));
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Argument, 0))),
      (IntSetPathTree{
          {Path{y}, IntSet{3}},
      }));
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Argument, 0), Path{y})),
      (IntSetPathTree{IntSet{3}}));
  EXPECT_EQ(
      tree.read(AccessPath(Root(Root::Kind::Argument, 1))),
      (IntSetPathTree{IntSet{4}}));
}

TEST_F(AccessPathTreeDomainTest, RawRead) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  auto tree = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1}},
      {AccessPath(Root(Root::Kind::Return), Path{x}), IntSet{2}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{y}), IntSet{3}},
      {AccessPath(Root(Root::Kind::Argument, 1)), IntSet{4}},
  };
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Return))),
      (IntSetPathTree{
          {Path{}, IntSet{1}},
          {Path{x}, IntSet{2}},
      }));
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Return), Path{x})),
      (IntSetPathTree{IntSet{2}}));
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Return), Path{x, y})),
      IntSetPathTree::bottom());
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Return), Path{y})),
      IntSetPathTree::bottom());
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Argument, 0))),
      (IntSetPathTree{
          {Path{y}, IntSet{3}},
      }));
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Argument, 0), Path{y})),
      (IntSetPathTree{IntSet{3}}));
  EXPECT_EQ(
      tree.raw_read(AccessPath(Root(Root::Kind::Argument, 1))),
      (IntSetPathTree{IntSet{4}}));
}

TEST_F(AccessPathTreeDomainTest, LessOrEqual) {
  const auto x = PathElement::field("x");

  EXPECT_TRUE(
      IntSetAccessPathTree::bottom().leq(IntSetAccessPathTree::bottom()));
  EXPECT_TRUE(IntSetAccessPathTree().leq(IntSetAccessPathTree::bottom()));

  EXPECT_TRUE(IntSetAccessPathTree::bottom().leq(IntSetAccessPathTree()));
  EXPECT_TRUE(IntSetAccessPathTree().leq(IntSetAccessPathTree()));

  auto tree1 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1}},
  };
  EXPECT_FALSE(tree1.leq(IntSetAccessPathTree::bottom()));
  EXPECT_FALSE(tree1.leq(IntSetAccessPathTree{}));
  EXPECT_TRUE(IntSetAccessPathTree::bottom().leq(tree1));
  EXPECT_TRUE(IntSetAccessPathTree().leq(tree1));
  EXPECT_TRUE(tree1.leq(tree1));

  auto tree2 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1, 2}},
  };
  EXPECT_TRUE(tree1.leq(tree2));
  EXPECT_FALSE(tree2.leq(tree1));
  EXPECT_TRUE(tree2.leq(tree2));

  auto tree3 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{2, 3}},
  };
  EXPECT_FALSE(tree1.leq(tree3));
  EXPECT_FALSE(tree2.leq(tree3));
  EXPECT_FALSE(tree3.leq(tree1));
  EXPECT_FALSE(tree3.leq(tree2));

  auto tree4 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1}},
      {AccessPath(Root(Root::Kind::Return), Path{x}), IntSet{2}},
  };
  EXPECT_TRUE(tree1.leq(tree4));
  EXPECT_FALSE(tree4.leq(tree1));
  EXPECT_FALSE(tree2.leq(tree4));
  EXPECT_TRUE(tree4.leq(tree2));
  EXPECT_FALSE(tree3.leq(tree4));
  EXPECT_FALSE(tree4.leq(tree3));

  auto tree5 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1, 2}},
      {AccessPath(Root(Root::Kind::Argument, 0)), IntSet{3}},
  };
  EXPECT_TRUE(tree1.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree1));
  EXPECT_TRUE(tree2.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree2));
  EXPECT_FALSE(tree3.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree3));
  EXPECT_TRUE(tree4.leq(tree5));
  EXPECT_FALSE(tree5.leq(tree4));

  auto tree6 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1, 2, 3}},
      {AccessPath(Root(Root::Kind::Argument, 0)), IntSet{3, 4}},
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

  auto tree7 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Argument, 0)), IntSet{4}},
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
}

TEST_F(AccessPathTreeDomainTest, Equal) {
  const auto x = PathElement::field("x");

  EXPECT_TRUE(
      IntSetAccessPathTree::bottom().equals(IntSetAccessPathTree::bottom()));
  EXPECT_TRUE(IntSetAccessPathTree().equals(IntSetAccessPathTree::bottom()));
  EXPECT_TRUE(IntSetAccessPathTree::bottom().equals(IntSetAccessPathTree()));
  EXPECT_TRUE(IntSetAccessPathTree().equals(IntSetAccessPathTree()));

  auto tree1 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1}},
  };
  EXPECT_FALSE(tree1.equals(IntSetAccessPathTree::bottom()));
  EXPECT_FALSE(IntSetAccessPathTree::bottom().equals(tree1));
  EXPECT_TRUE(tree1.equals(tree1));

  auto tree2 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1, 2}},
  };
  EXPECT_FALSE(tree1.equals(tree2));
  EXPECT_TRUE(tree2.equals(tree2));

  auto tree3 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{2, 3}},
  };
  EXPECT_FALSE(tree1.equals(tree3));
  EXPECT_FALSE(tree2.equals(tree3));
  EXPECT_TRUE(tree3.equals(tree3));

  auto tree4 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1}},
      {AccessPath(Root(Root::Kind::Return), Path{x}), IntSet{2}},
  };
  EXPECT_FALSE(tree1.equals(tree4));
  EXPECT_FALSE(tree2.equals(tree4));
  EXPECT_FALSE(tree3.equals(tree4));
  EXPECT_TRUE(tree4.equals(tree4));

  auto tree5 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1, 2}},
      {AccessPath(Root(Root::Kind::Argument, 0)), IntSet{3}},
  };
  EXPECT_FALSE(tree1.equals(tree5));
  EXPECT_FALSE(tree2.equals(tree5));
  EXPECT_FALSE(tree3.equals(tree5));
  EXPECT_FALSE(tree4.equals(tree5));
  EXPECT_TRUE(tree5.equals(tree5));

  auto tree6 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1, 2, 3}},
      {AccessPath(Root(Root::Kind::Argument, 0)), IntSet{3, 4}},
  };
  EXPECT_FALSE(tree1.equals(tree6));
  EXPECT_FALSE(tree2.equals(tree6));
  EXPECT_FALSE(tree3.equals(tree6));
  EXPECT_FALSE(tree4.equals(tree6));
  EXPECT_FALSE(tree5.equals(tree6));
  EXPECT_TRUE(tree6.equals(tree6));

  auto tree7 = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Argument, 0)), IntSet{4}},
  };
  EXPECT_FALSE(tree1.equals(tree7));
  EXPECT_FALSE(tree2.equals(tree7));
  EXPECT_FALSE(tree3.equals(tree7));
  EXPECT_FALSE(tree4.equals(tree7));
  EXPECT_FALSE(tree5.equals(tree7));
  EXPECT_FALSE(tree6.equals(tree7));
  EXPECT_TRUE(tree7.equals(tree7));
}

TEST_F(AccessPathTreeDomainTest, Join) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetAccessPathTree::bottom();
  tree.join_with(IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1}},
  });
  EXPECT_EQ(
      tree,
      (IntSetAccessPathTree{
          {AccessPath(Root(Root::Kind::Return)), IntSet{1}},
      }));

  tree.join_with(IntSetAccessPathTree::bottom());
  EXPECT_EQ(
      tree,
      (IntSetAccessPathTree{
          {AccessPath(Root(Root::Kind::Return)), IntSet{1}},
      }));

  tree.join_with(IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{2}},
  });
  EXPECT_EQ(
      tree,
      (IntSetAccessPathTree{
          {AccessPath(Root(Root::Kind::Return)), IntSet{1, 2}},
      }));

  tree = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1, 2, 3}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}), IntSet{3, 4}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}), IntSet{5, 6}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}), IntSet{7, 8}},
      {AccessPath(Root(Root::Kind::Argument, 1)), IntSet{10}},
  };
  tree.join_with(IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return), Path{x}), IntSet{1}},
      {AccessPath(Root(Root::Kind::Return), Path{y}), IntSet{2}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}), IntSet{6, 7}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, x}), IntSet{8, 9}},
      {AccessPath(Root(Root::Kind::Argument, 2)), IntSet{20}},
  });
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Return)), (IntSetPathTree{IntSet{1, 2, 3}}));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 0)),
      (IntSetPathTree{
          {Path{x}, IntSet{3, 4, 6, 7}},
          {Path{x, y}, IntSet{5}},
          {Path{x, z}, IntSet{8}},
          {Path{x, x}, IntSet{8, 9}},
      }));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 1)), (IntSetPathTree{IntSet{10}}));
  EXPECT_EQ(
      tree.read(Root(Root::Kind::Argument, 2)), (IntSetPathTree{IntSet{20}}));
  EXPECT_TRUE(tree.read(Root(Root::Kind::Argument, 3)).is_bottom());
}

TEST_F(AccessPathTreeDomainTest, Elements) {
  using Pair = std::pair<AccessPath, IntSet>;

  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetAccessPathTree::bottom();
  EXPECT_TRUE(tree.elements().empty());

  tree = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1}},
  };
  EXPECT_THAT(
      tree.elements(),
      testing::UnorderedElementsAre(
          Pair{AccessPath(Root(Root::Kind::Return)), IntSet{1}}));

  tree = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1, 2}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}), IntSet{1, 2}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}), IntSet{3, 4}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}), IntSet{5, 6}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z, y}), IntSet{7, 8}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, x}), IntSet{9, 10}},
      {AccessPath(Root(Root::Kind::Argument, 1), Path{x, y}), IntSet{1, 2}},
      {AccessPath(Root(Root::Kind::Argument, 2)), IntSet{1, 2}},
      {AccessPath(Root(Root::Kind::Argument, 2), Path{x, y}), IntSet{3, 4}},
  };
  EXPECT_THAT(
      tree.elements(),
      testing::UnorderedElementsAre(
          Pair{AccessPath(Root(Root::Kind::Return)), IntSet{1, 2}},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 0), Path{x}), IntSet{1, 2}},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
              IntSet{3, 4}},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}),
              IntSet{5, 6}},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 0), Path{x, z, y}),
              IntSet{7, 8}},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 0), Path{x, x}),
              IntSet{9, 10}},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 1), Path{x, y}),
              IntSet{1, 2}},
          Pair{AccessPath(Root(Root::Kind::Argument, 2)), IntSet{1, 2}},
          Pair{
              AccessPath(Root(Root::Kind::Argument, 2), Path{x, y}),
              IntSet{3, 4}}));
}

TEST_F(AccessPathTreeDomainTest, Map) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1, 2}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}), IntSet{1, 2}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}), IntSet{3, 4}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}), IntSet{5, 6}},
      {AccessPath(Root(Root::Kind::Argument, 1), Path{x, y}), IntSet{1, 2}},
      {AccessPath(Root(Root::Kind::Argument, 2)), IntSet{1, 2}},
      {AccessPath(Root(Root::Kind::Argument, 2), Path{x, y}), IntSet{3, 4}},
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
      (IntSetAccessPathTree{
          {AccessPath(Root(Root::Kind::Return)), IntSet{1, 4}},
          {AccessPath(Root(Root::Kind::Argument, 0), Path{x}), IntSet{1, 4}},
          {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
           IntSet{9, 16}},
          {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}),
           IntSet{25, 36}},
          {AccessPath(Root(Root::Kind::Argument, 1), Path{x, y}), IntSet{1, 4}},
          {AccessPath(Root(Root::Kind::Argument, 2)), IntSet{1, 4}},
          {AccessPath(Root(Root::Kind::Argument, 2), Path{x, y}),
           IntSet{9, 16}},
      }));
}

TEST_F(AccessPathTreeDomainTest, CollapseInvalid) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  auto tree = IntSetAccessPathTree{
      {AccessPath(Root(Root::Kind::Return)), IntSet{1}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x}), IntSet{2}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}), IntSet{3}},
      {AccessPath(Root(Root::Kind::Argument, 0), Path{x, z}), IntSet{4}},
      {AccessPath(Root(Root::Kind::Argument, 1)), IntSet{5}},
      {AccessPath(Root(Root::Kind::Argument, 1), Path{x}), IntSet{6}},
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

  auto identity = [](IntSet&) {};

  tree.collapse_invalid_paths<Accumulator>(
      is_valid, initial_accumulator, identity);
  EXPECT_EQ(
      tree,
      (IntSetAccessPathTree{
          {AccessPath(Root(Root::Kind::Return)), IntSet{1}},
          {AccessPath(Root(Root::Kind::Argument, 0), Path{x}), IntSet{2, 3, 4}},
          {AccessPath(Root(Root::Kind::Argument, 1)), IntSet{5, 6}},
      }));
}

} // namespace marianatrench
