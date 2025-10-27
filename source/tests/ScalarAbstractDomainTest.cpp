/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/ScalarAbstractDomain.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class ScalarAbstractDomainTest : public test::Test {};

using ScalarBottomIsZeroAbstractDomain =
    ScalarAbstractDomainScaffolding<scalar_impl::ScalarBottomIsZero>;
using ScalarTopIsZeroAbstractDomain =
    ScalarAbstractDomainScaffolding<scalar_impl::ScalarTopIsZero>;

TEST_F(ScalarAbstractDomainTest, DefaultConstructor) {
  EXPECT_TRUE(ScalarBottomIsZeroAbstractDomain().is_bottom());
  EXPECT_TRUE(ScalarBottomIsZeroAbstractDomain(
                  ScalarBottomIsZeroAbstractDomain::Enum::Bottom)
                  .is_bottom());
  EXPECT_TRUE(ScalarBottomIsZeroAbstractDomain::bottom().is_bottom());
  EXPECT_TRUE(ScalarBottomIsZeroAbstractDomain::top().is_top());
  EXPECT_EQ(
      ScalarBottomIsZeroAbstractDomain(),
      ScalarBottomIsZeroAbstractDomain(
          ScalarBottomIsZeroAbstractDomain::Enum::Zero));
  EXPECT_EQ(
      ScalarBottomIsZeroAbstractDomain::top(),
      ScalarBottomIsZeroAbstractDomain(
          ScalarBottomIsZeroAbstractDomain::Enum::Top));

  EXPECT_TRUE(ScalarTopIsZeroAbstractDomain().is_bottom());
  EXPECT_TRUE(
      ScalarTopIsZeroAbstractDomain(ScalarTopIsZeroAbstractDomain::Enum::Bottom)
          .is_bottom());
  EXPECT_TRUE(ScalarTopIsZeroAbstractDomain::bottom().is_bottom());
  EXPECT_TRUE(ScalarTopIsZeroAbstractDomain::top().is_top());
  EXPECT_EQ(
      ScalarTopIsZeroAbstractDomain(),
      ScalarTopIsZeroAbstractDomain(
          ScalarTopIsZeroAbstractDomain::Enum::Bottom));
  EXPECT_EQ(
      ScalarTopIsZeroAbstractDomain::top(),
      ScalarTopIsZeroAbstractDomain(ScalarTopIsZeroAbstractDomain::Enum::Zero));
}

TEST_F(ScalarAbstractDomainTest, BottomIsZeroJoinWith) {
  // join is max
  auto bottom_is_zero = ScalarBottomIsZeroAbstractDomain(1);
  EXPECT_EQ(
      bottom_is_zero.join(ScalarBottomIsZeroAbstractDomain::bottom()),
      ScalarBottomIsZeroAbstractDomain(1));

  bottom_is_zero.join_with(ScalarBottomIsZeroAbstractDomain::bottom());
  EXPECT_EQ(bottom_is_zero, ScalarBottomIsZeroAbstractDomain(1));

  EXPECT_EQ(
      bottom_is_zero.join(ScalarBottomIsZeroAbstractDomain(2)),
      ScalarBottomIsZeroAbstractDomain(2));

  EXPECT_TRUE(
      bottom_is_zero.join(ScalarBottomIsZeroAbstractDomain::top()).is_top());
}

TEST_F(ScalarAbstractDomainTest, TopIsZeroJoinWith) {
  // join is min
  auto top_is_zero = ScalarTopIsZeroAbstractDomain(42);
  EXPECT_EQ(
      top_is_zero.join(ScalarTopIsZeroAbstractDomain::bottom()),
      ScalarTopIsZeroAbstractDomain(42));

  top_is_zero.join_with(ScalarTopIsZeroAbstractDomain::bottom());
  EXPECT_EQ(top_is_zero, ScalarTopIsZeroAbstractDomain(42));

  EXPECT_EQ(
      top_is_zero.join(ScalarTopIsZeroAbstractDomain(1)),
      ScalarTopIsZeroAbstractDomain(1));

  EXPECT_TRUE(top_is_zero.join(ScalarTopIsZeroAbstractDomain::top()).is_top());
}

TEST_F(ScalarAbstractDomainTest, BottomIsZeroMeetWith) {
  // meet is min
  auto bottom_is_zero = ScalarBottomIsZeroAbstractDomain(1);
  EXPECT_EQ(
      bottom_is_zero.meet(ScalarBottomIsZeroAbstractDomain::bottom()),
      ScalarBottomIsZeroAbstractDomain::bottom());

  bottom_is_zero.meet_with(ScalarBottomIsZeroAbstractDomain::bottom());
  EXPECT_EQ(bottom_is_zero, ScalarBottomIsZeroAbstractDomain::bottom());
  EXPECT_TRUE(bottom_is_zero.is_bottom());

  EXPECT_TRUE(
      bottom_is_zero.meet(ScalarBottomIsZeroAbstractDomain::top()).is_bottom());

  EXPECT_EQ(
      ScalarBottomIsZeroAbstractDomain(1).meet(
          ScalarBottomIsZeroAbstractDomain(2)),
      ScalarBottomIsZeroAbstractDomain(1));
}

TEST_F(ScalarAbstractDomainTest, TopIsZeroMeetWith) {
  // meet is max
  auto top_is_zero = ScalarTopIsZeroAbstractDomain(42);
  EXPECT_EQ(
      top_is_zero.meet(ScalarTopIsZeroAbstractDomain::bottom()),
      ScalarTopIsZeroAbstractDomain::bottom());

  top_is_zero.meet_with(ScalarTopIsZeroAbstractDomain::bottom());
  EXPECT_EQ(top_is_zero, ScalarTopIsZeroAbstractDomain::bottom());
  EXPECT_TRUE(top_is_zero.is_bottom());

  EXPECT_EQ(top_is_zero.meet(ScalarTopIsZeroAbstractDomain(1)), top_is_zero);

  EXPECT_EQ(
      top_is_zero.meet(ScalarTopIsZeroAbstractDomain::top()), top_is_zero);

  EXPECT_EQ(
      ScalarTopIsZeroAbstractDomain(1).meet(ScalarTopIsZeroAbstractDomain(2)),
      ScalarTopIsZeroAbstractDomain(2));
}

TEST_F(ScalarAbstractDomainTest, BottomIsZeroLessOrEqual) {
  EXPECT_TRUE(
      ScalarBottomIsZeroAbstractDomain().leq(
          ScalarBottomIsZeroAbstractDomain::bottom()));
  EXPECT_TRUE(
      ScalarBottomIsZeroAbstractDomain::bottom().leq(
          ScalarBottomIsZeroAbstractDomain()));

  auto bottom_is_zero = ScalarBottomIsZeroAbstractDomain(1);
  EXPECT_FALSE(bottom_is_zero.leq(ScalarBottomIsZeroAbstractDomain()));
  EXPECT_FALSE(bottom_is_zero.leq(ScalarBottomIsZeroAbstractDomain::bottom()));

  EXPECT_TRUE(bottom_is_zero.leq(ScalarBottomIsZeroAbstractDomain::top()));
  EXPECT_TRUE(bottom_is_zero.leq(ScalarBottomIsZeroAbstractDomain(
      ScalarBottomIsZeroAbstractDomain::Enum::Top)));
  EXPECT_TRUE(bottom_is_zero.leq(ScalarBottomIsZeroAbstractDomain(
      ScalarBottomIsZeroAbstractDomain::Enum::Max)));

  EXPECT_TRUE(bottom_is_zero.leq(ScalarBottomIsZeroAbstractDomain(2)));
}

TEST_F(ScalarAbstractDomainTest, TopIsZeroLessOrEqual) {
  EXPECT_TRUE(
      ScalarTopIsZeroAbstractDomain().leq(
          ScalarTopIsZeroAbstractDomain::bottom()));
  EXPECT_TRUE(
      ScalarTopIsZeroAbstractDomain::bottom().leq(
          ScalarTopIsZeroAbstractDomain()));

  auto top_is_zero = ScalarTopIsZeroAbstractDomain(42);
  EXPECT_FALSE(top_is_zero.leq(ScalarTopIsZeroAbstractDomain()));
  EXPECT_FALSE(top_is_zero.leq(ScalarTopIsZeroAbstractDomain::bottom()));

  EXPECT_TRUE(top_is_zero.leq(ScalarTopIsZeroAbstractDomain::top()));
  EXPECT_TRUE(top_is_zero.leq(
      ScalarTopIsZeroAbstractDomain(ScalarTopIsZeroAbstractDomain::Enum::Top)));
  EXPECT_FALSE(top_is_zero.leq(
      ScalarTopIsZeroAbstractDomain(ScalarTopIsZeroAbstractDomain::Enum::Max)));

  EXPECT_TRUE(top_is_zero.leq(ScalarTopIsZeroAbstractDomain(1)));
}

TEST_F(ScalarAbstractDomainTest, BottomIsZeroEquals) {
  EXPECT_TRUE(
      ScalarBottomIsZeroAbstractDomain().equals(
          ScalarBottomIsZeroAbstractDomain::bottom()));
  EXPECT_TRUE(
      ScalarBottomIsZeroAbstractDomain::bottom().equals(
          ScalarBottomIsZeroAbstractDomain()));
  EXPECT_TRUE(
      ScalarBottomIsZeroAbstractDomain::bottom().equals(
          ScalarBottomIsZeroAbstractDomain(
              ScalarBottomIsZeroAbstractDomain::Enum::Zero)));

  EXPECT_TRUE(ScalarBottomIsZeroAbstractDomain(
                  ScalarBottomIsZeroAbstractDomain::Enum::Top)
                  .equals(ScalarBottomIsZeroAbstractDomain::top()));
  EXPECT_TRUE(
      ScalarBottomIsZeroAbstractDomain::top().equals(
          ScalarBottomIsZeroAbstractDomain(
              ScalarBottomIsZeroAbstractDomain::Enum::Top)));

  auto bottom_is_zero = ScalarBottomIsZeroAbstractDomain(1);
  EXPECT_FALSE(bottom_is_zero.equals(ScalarBottomIsZeroAbstractDomain()));
  EXPECT_FALSE(
      bottom_is_zero.equals(ScalarBottomIsZeroAbstractDomain::bottom()));

  EXPECT_FALSE(bottom_is_zero.equals(ScalarBottomIsZeroAbstractDomain::top()));
  EXPECT_FALSE(bottom_is_zero.equals(ScalarBottomIsZeroAbstractDomain(
      ScalarBottomIsZeroAbstractDomain::Enum::Top)));
  EXPECT_FALSE(bottom_is_zero.equals(ScalarBottomIsZeroAbstractDomain(
      ScalarBottomIsZeroAbstractDomain::Enum::Max)));

  EXPECT_TRUE(bottom_is_zero.equals(ScalarBottomIsZeroAbstractDomain(1)));
  EXPECT_FALSE(bottom_is_zero.equals(ScalarBottomIsZeroAbstractDomain(2)));
}

TEST_F(ScalarAbstractDomainTest, TopIsZeroEquals) {
  EXPECT_TRUE(
      ScalarTopIsZeroAbstractDomain().equals(
          ScalarTopIsZeroAbstractDomain::bottom()));
  EXPECT_TRUE(
      ScalarTopIsZeroAbstractDomain::bottom().equals(
          ScalarTopIsZeroAbstractDomain()));

  EXPECT_TRUE(
      ScalarTopIsZeroAbstractDomain(ScalarTopIsZeroAbstractDomain::Enum::Top)
          .equals(ScalarTopIsZeroAbstractDomain::top()));
  EXPECT_TRUE(
      ScalarTopIsZeroAbstractDomain::top().equals(ScalarTopIsZeroAbstractDomain(
          ScalarTopIsZeroAbstractDomain::Enum::Top)));
  EXPECT_TRUE(
      ScalarTopIsZeroAbstractDomain::top().equals(ScalarTopIsZeroAbstractDomain(
          ScalarTopIsZeroAbstractDomain::Enum::Zero)));

  auto top_is_zero = ScalarTopIsZeroAbstractDomain(1);
  EXPECT_FALSE(top_is_zero.equals(ScalarTopIsZeroAbstractDomain()));
  EXPECT_FALSE(top_is_zero.equals(ScalarTopIsZeroAbstractDomain::bottom()));

  EXPECT_FALSE(top_is_zero.equals(ScalarTopIsZeroAbstractDomain::top()));
  EXPECT_FALSE(top_is_zero.equals(
      ScalarTopIsZeroAbstractDomain(ScalarTopIsZeroAbstractDomain::Enum::Top)));
  EXPECT_FALSE(top_is_zero.equals(
      ScalarTopIsZeroAbstractDomain(ScalarTopIsZeroAbstractDomain::Enum::Max)));

  EXPECT_TRUE(top_is_zero.equals(ScalarTopIsZeroAbstractDomain(1)));
  EXPECT_FALSE(top_is_zero.equals(ScalarTopIsZeroAbstractDomain(2)));
}

} // namespace marianatrench
