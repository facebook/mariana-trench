/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/LocalPositionSet.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class LocalPositionSetTest : public test::Test {};

TEST_F(LocalPositionSetTest, Constructor) {
  auto context = test::make_empty_context();

  EXPECT_TRUE(LocalPositionSet::bottom().is_bottom());
  EXPECT_TRUE(LocalPositionSet::top().is_top());
  EXPECT_TRUE(LocalPositionSet().empty());
  EXPECT_TRUE(
      (LocalPositionSet{context.positions->get(std::nullopt, 1)}).is_value());
}

TEST_F(LocalPositionSetTest, Leq) {
  auto context = test::make_empty_context();
  const auto* one = context.positions->get(std::nullopt, 1);
  const auto* two = context.positions->get(std::nullopt, 2);

  EXPECT_FALSE((LocalPositionSet{one}).leq(LocalPositionSet::bottom()));
  EXPECT_TRUE((LocalPositionSet{one}).leq(LocalPositionSet::top()));
  EXPECT_FALSE((LocalPositionSet{one}).leq(LocalPositionSet{}));
  EXPECT_TRUE((LocalPositionSet{one}).leq(LocalPositionSet{one}));
  EXPECT_TRUE((LocalPositionSet{one}).leq(LocalPositionSet{one, two}));

  EXPECT_FALSE((LocalPositionSet{}).leq(LocalPositionSet::bottom()));
  EXPECT_TRUE((LocalPositionSet{}).leq(LocalPositionSet::top()));
  EXPECT_TRUE((LocalPositionSet{}).leq(LocalPositionSet{}));
  EXPECT_TRUE((LocalPositionSet{}).leq(LocalPositionSet{one}));

  EXPECT_FALSE((LocalPositionSet{one, two}).leq(LocalPositionSet{}));
  EXPECT_FALSE((LocalPositionSet{one, two}).leq(LocalPositionSet{one}));
  EXPECT_TRUE((LocalPositionSet{one, two}).leq(LocalPositionSet{one, two}));
}

TEST_F(LocalPositionSetTest, Equals) {
  auto context = test::make_empty_context();
  const auto* one = context.positions->get(std::nullopt, 1);
  const auto* two = context.positions->get(std::nullopt, 2);

  EXPECT_FALSE((LocalPositionSet{one}).equals(LocalPositionSet::bottom()));
  EXPECT_FALSE((LocalPositionSet{one}).equals(LocalPositionSet::top()));
  EXPECT_FALSE((LocalPositionSet{one}).equals(LocalPositionSet{}));
  EXPECT_TRUE((LocalPositionSet{one}).equals(LocalPositionSet{one}));
  EXPECT_FALSE((LocalPositionSet{one}).equals(LocalPositionSet{one, two}));

  EXPECT_FALSE((LocalPositionSet{}).equals(LocalPositionSet::bottom()));
  EXPECT_FALSE((LocalPositionSet{}).equals(LocalPositionSet::top()));
  EXPECT_TRUE((LocalPositionSet{}).equals(LocalPositionSet{}));
  EXPECT_FALSE((LocalPositionSet{}).equals(LocalPositionSet{one}));

  EXPECT_FALSE((LocalPositionSet{one, two}).equals(LocalPositionSet{}));
  EXPECT_FALSE((LocalPositionSet{one, two}).equals(LocalPositionSet{one}));
  EXPECT_TRUE((LocalPositionSet{one, two}).equals(LocalPositionSet{one, two}));
}

TEST_F(LocalPositionSetTest, Join) {
  auto context = test::make_empty_context();
  const auto* one = context.positions->get(std::nullopt, 1);
  const auto* two = context.positions->get(std::nullopt, 2);

  EXPECT_EQ(
      (LocalPositionSet{one}).join(LocalPositionSet::bottom()),
      LocalPositionSet{one});
  EXPECT_EQ(
      (LocalPositionSet{one}).join(LocalPositionSet::top()),
      LocalPositionSet::top());
  EXPECT_EQ(
      (LocalPositionSet{one}).join(LocalPositionSet{}), LocalPositionSet{one});
  EXPECT_EQ(
      (LocalPositionSet{one}).join(LocalPositionSet{one}),
      LocalPositionSet{one});
  EXPECT_EQ(
      (LocalPositionSet{one}).join(LocalPositionSet{two}),
      (LocalPositionSet{one, two}));
  EXPECT_EQ(
      (LocalPositionSet{one}).join(LocalPositionSet{one, two}),
      (LocalPositionSet{one, two}));

  EXPECT_EQ(
      (LocalPositionSet{}).join(LocalPositionSet::bottom()),
      LocalPositionSet{});
  EXPECT_EQ(
      (LocalPositionSet{}).join(LocalPositionSet::top()),
      LocalPositionSet::top());
  EXPECT_EQ((LocalPositionSet{}).join(LocalPositionSet{}), LocalPositionSet{});
  EXPECT_EQ(
      (LocalPositionSet{}).join(LocalPositionSet{one}), LocalPositionSet{one});

  EXPECT_EQ(
      (LocalPositionSet{one, two}).join(LocalPositionSet{}),
      (LocalPositionSet{one, two}));
  EXPECT_EQ(
      (LocalPositionSet{one, two}).join(LocalPositionSet{one}),
      (LocalPositionSet{one, two}));
  EXPECT_EQ(
      (LocalPositionSet{one, two}).join(LocalPositionSet{one, two}),
      (LocalPositionSet{one, two}));

  auto set = LocalPositionSet{};
  for (std::size_t i = 0; i < Heuristics::kMaxNumberLocalPositions; i++) {
    set.join_with(LocalPositionSet{context.positions->get(std::nullopt, i)});
  }
  EXPECT_TRUE(set.is_value());
  EXPECT_EQ(set.elements().size(), Heuristics::kMaxNumberLocalPositions);

  set.join_with(
      LocalPositionSet{context.positions->get(
          std::nullopt, Heuristics::kMaxNumberLocalPositions)});
  EXPECT_TRUE(set.is_top());
}

TEST_F(LocalPositionSetTest, Add) {
  auto context = test::make_empty_context();
  const auto* one = context.positions->get(std::nullopt, 1);
  const auto* two = context.positions->get(std::nullopt, 2);

  auto set = LocalPositionSet::bottom();
  set.add(one);
  EXPECT_EQ(set, LocalPositionSet::bottom());

  set = LocalPositionSet::top();
  set.add(one);
  EXPECT_EQ(set, LocalPositionSet::top());

  set = LocalPositionSet();
  set.add(one);
  EXPECT_EQ(set, LocalPositionSet{one});
  set.add(two);
  EXPECT_EQ(set, (LocalPositionSet{one, two}));

  set = LocalPositionSet();
  for (std::size_t i = 0; i < Heuristics::kMaxNumberLocalPositions; i++) {
    set.add(context.positions->get(std::nullopt, i));
  }
  EXPECT_TRUE(set.is_value());
  EXPECT_EQ(set.elements().size(), Heuristics::kMaxNumberLocalPositions);

  set.add(context.positions->get(
      std::nullopt, Heuristics::kMaxNumberLocalPositions));
  EXPECT_TRUE(set.is_top());
}

} // namespace marianatrench
