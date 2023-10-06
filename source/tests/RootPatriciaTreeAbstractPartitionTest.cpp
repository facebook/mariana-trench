/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <sparta/PatriciaTreeSetAbstractDomain.h>

#include <mariana-trench/RootPatriciaTreeAbstractPartition.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class RootPatriciaTreeAbstractPartitionTest : public test::Test {};

using IntSet = sparta::PatriciaTreeSetAbstractDomain<unsigned>;
using RootToIntSetPartition = RootPatriciaTreeAbstractPartition<IntSet>;

TEST_F(RootPatriciaTreeAbstractPartitionTest, DefaultConstructor) {
  EXPECT_TRUE(RootToIntSetPartition().is_bottom());
}

TEST_F(RootPatriciaTreeAbstractPartitionTest, Constructor) {
  auto map = RootToIntSetPartition{
      {Root(Root::Kind::Return), IntSet{1}},
      {Root(Root::Kind::Argument, 1), IntSet{2}},
  };
  EXPECT_FALSE(map.is_bottom());
  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map.get(Root(Root::Kind::Return)), IntSet{1});
  EXPECT_EQ(map.get(Root(Root::Kind::Argument, 1)), IntSet{2});
}

TEST_F(RootPatriciaTreeAbstractPartitionTest, Set) {
  auto map = RootToIntSetPartition();
  EXPECT_TRUE(map.is_bottom());

  map.set(Root(Root::Kind::Return), IntSet{1});
  EXPECT_FALSE(map.is_bottom());
  EXPECT_EQ(map.size(), 1);
  EXPECT_EQ(map.get(Root(Root::Kind::Return)), IntSet{1});

  map.set(Root(Root::Kind::Argument, 1), IntSet{2});
  EXPECT_FALSE(map.is_bottom());
  EXPECT_EQ(map.size(), 2);
  EXPECT_EQ(map.get(Root(Root::Kind::Return)), IntSet{1});
  EXPECT_EQ(map.get(Root(Root::Kind::Argument, 1)), IntSet{2});
}

TEST_F(RootPatriciaTreeAbstractPartitionTest, LessOrEqual) {
  EXPECT_TRUE(
      RootToIntSetPartition::bottom().leq(RootToIntSetPartition::bottom()));
  EXPECT_TRUE(RootToIntSetPartition().leq(RootToIntSetPartition::bottom()));

  EXPECT_TRUE(RootToIntSetPartition::bottom().leq(RootToIntSetPartition()));
  EXPECT_TRUE(RootToIntSetPartition().leq(RootToIntSetPartition()));

  auto map1 = RootToIntSetPartition{
      {Root(Root::Kind::Return), IntSet{1}},
  };
  EXPECT_FALSE(map1.leq(RootToIntSetPartition::bottom()));
  EXPECT_FALSE(map1.leq(RootToIntSetPartition{}));
  EXPECT_TRUE(RootToIntSetPartition::bottom().leq(map1));
  EXPECT_TRUE(RootToIntSetPartition().leq(map1));
  EXPECT_TRUE(map1.leq(map1));

  auto map2 = RootToIntSetPartition{
      {Root(Root::Kind::Return), IntSet{1, 2}},
  };
  EXPECT_TRUE(map1.leq(map2));
  EXPECT_FALSE(map2.leq(map1));
  EXPECT_TRUE(map2.leq(map2));

  auto map3 = RootToIntSetPartition{
      {Root(Root::Kind::Return), IntSet{2, 3}},
  };
  EXPECT_FALSE(map1.leq(map3));
  EXPECT_FALSE(map3.leq(map1));
  EXPECT_FALSE(map2.leq(map3));
  EXPECT_FALSE(map3.leq(map2));
  EXPECT_TRUE(map3.leq(map3));

  auto map4 = RootToIntSetPartition{
      {Root(Root::Kind::Return), IntSet{1, 2, 3}},
      {Root(Root::Kind::Argument, 0), IntSet{0}},
  };
  EXPECT_TRUE(map1.leq(map4));
  EXPECT_FALSE(map4.leq(map1));
  EXPECT_TRUE(map2.leq(map4));
  EXPECT_FALSE(map4.leq(map2));
  EXPECT_TRUE(map3.leq(map4));
  EXPECT_FALSE(map4.leq(map3));

  auto map5 = RootToIntSetPartition{
      {Root(Root::Kind::Return), IntSet{1, 2, 3}},
      {Root(Root::Kind::Argument, 1), IntSet{0}},
  };
  EXPECT_FALSE(map4.leq(map5));
  EXPECT_FALSE(map5.leq(map4));
}

TEST_F(RootPatriciaTreeAbstractPartitionTest, Join) {
  auto map = RootToIntSetPartition{
      {Root(Root::Kind::Return), IntSet{1}},
      {Root(Root::Kind::Argument, 0), IntSet{2}},
  };
  map.join_with(RootToIntSetPartition{
      {Root(Root::Kind::Return), IntSet{2}},
      {Root(Root::Kind::Argument, 1), IntSet{3}},
  });
  EXPECT_EQ(
      map,
      (RootToIntSetPartition{
          {Root(Root::Kind::Return), IntSet{1, 2}},
          {Root(Root::Kind::Argument, 0), IntSet{2}},
          {Root(Root::Kind::Argument, 1), IntSet{3}},
      }));
}

TEST_F(RootPatriciaTreeAbstractPartitionTest, Difference) {
  auto map = RootToIntSetPartition{
      {Root(Root::Kind::Return), IntSet{1}},
      {Root(Root::Kind::Argument, 0), IntSet{2}},
  };

  map.difference_with(RootToIntSetPartition::bottom());
  EXPECT_EQ(
      map,
      (RootToIntSetPartition{
          {Root(Root::Kind::Return), IntSet{1}},
          {Root(Root::Kind::Argument, 0), IntSet{2}},
      }));

  map.difference_with(
      RootToIntSetPartition{{Root(Root::Kind::Return), IntSet{1}}});
  EXPECT_EQ(
      map, (RootToIntSetPartition{{Root(Root::Kind::Argument, 0), IntSet{2}}}));

  // Current value is not leq value in object being 'subtracted'
  map.difference_with(
      RootToIntSetPartition{{Root(Root::Kind::Argument, 0), IntSet{3}}});
  EXPECT_EQ(
      map, (RootToIntSetPartition{{Root(Root::Kind::Argument, 0), IntSet{2}}}));

  // Difference with a key that doesn't exist in the map
  map.difference_with(
      RootToIntSetPartition{{Root(Root::Kind::Argument, 1), IntSet{2}}});
  EXPECT_EQ(
      map, (RootToIntSetPartition{{Root(Root::Kind::Argument, 0), IntSet{2}}}));

  map.difference_with(
      RootToIntSetPartition{{Root(Root::Kind::Argument, 0), IntSet{2, 5}}});
  EXPECT_EQ(map, RootToIntSetPartition::bottom());
}

TEST_F(RootPatriciaTreeAbstractPartitionTest, Update) {
  auto map = RootToIntSetPartition{
      {Root(Root::Kind::Return), IntSet{1}},
      {Root(Root::Kind::Argument, 0), IntSet{2}},
  };
  map.update(Root(Root::Kind::Return), [](const IntSet& set) {
    auto copy = set;
    copy.add(10);
    return copy;
  });
  EXPECT_EQ(
      map,
      (RootToIntSetPartition{
          {Root(Root::Kind::Return), IntSet{1, 10}},
          {Root(Root::Kind::Argument, 0), IntSet{2}},
      }));
  map.update(Root(Root::Kind::Argument, 1), [](const IntSet& /*set*/) {
    return IntSet{10};
  });
  EXPECT_EQ(
      map,
      (RootToIntSetPartition{
          {Root(Root::Kind::Return), IntSet{1, 10}},
          {Root(Root::Kind::Argument, 0), IntSet{2}},
          {Root(Root::Kind::Argument, 1), IntSet{10}},
      }));
}

TEST_F(RootPatriciaTreeAbstractPartitionTest, Transform) {
  auto map = RootToIntSetPartition{
      {Root(Root::Kind::Return), IntSet{1}},
      {Root(Root::Kind::Argument, 0), IntSet{2}},
  };
  map.transform([](const IntSet& set) {
    auto copy = set;
    copy.add(10);
    return copy;
  });
  EXPECT_EQ(
      map,
      (RootToIntSetPartition{
          {Root(Root::Kind::Return), IntSet{1, 10}},
          {Root(Root::Kind::Argument, 0), IntSet{2, 10}},
      }));
}

TEST_F(RootPatriciaTreeAbstractPartitionTest, Iterator) {
  using Pair = std::pair<Root, IntSet>;
  std::vector<Pair> elements;

  auto map = RootToIntSetPartition{
      {Root(Root::Kind::Return), IntSet{1}},
  };

  elements = {map.begin(), map.end()};
  EXPECT_THAT(
      elements,
      testing::UnorderedElementsAre(Pair{Root(Root::Kind::Return), IntSet{1}}));

  map.set(Root(Root::Kind::Argument, 0), IntSet{2});
  elements = {map.begin(), map.end()};
  EXPECT_THAT(
      elements,
      testing::UnorderedElementsAre(
          Pair{Root(Root::Kind::Return), IntSet{1}},
          Pair{Root(Root::Kind::Argument, 0), IntSet{2}}));

  map.set(Root(Root::Kind::Argument, 1), IntSet{3});
  elements = {map.begin(), map.end()};
  EXPECT_THAT(
      elements,
      testing::UnorderedElementsAre(
          Pair{Root(Root::Kind::Return), IntSet{1}},
          Pair{Root(Root::Kind::Argument, 0), IntSet{2}},
          Pair{Root(Root::Kind::Argument, 1), IntSet{3}}));
}

} // namespace marianatrench
