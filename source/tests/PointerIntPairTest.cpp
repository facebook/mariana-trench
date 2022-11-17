/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <gtest/gtest.h>
#include <mariana-trench/PointerIntPair.h>
#include <mariana-trench/tests/Test.h>
#include <stdexcept>

namespace marianatrench {

class PointerIntPairTest : public test::Test {};

using IntType = size_t;

TEST_F(PointerIntPairTest, Constructors) {
  // Default constructor
  PointerIntPair<const DexString*, 3, IntType> pair_default;
  EXPECT_EQ(pair_default.get_int(), 0);
  EXPECT_EQ(pair_default.get_pointer(), nullptr);

  const auto* x = DexString::make_string("x");

  // Pointer only constructor
  PointerIntPair<const DexString*, 3, IntType> pair_one{x};
  EXPECT_EQ(pair_one.get_int(), 0);
  EXPECT_EQ(pair_one.get_pointer(), x);

  // Pointer and int constructor
  PointerIntPair<const DexString*, 3, IntType> pair{x, 1};

  EXPECT_EQ(pair.get_int(), 1);
  EXPECT_EQ(pair.get_pointer(), x);
}

TEST_F(PointerIntPairTest, SetInt) {
  const auto* x = DexString::make_string("x");
  PointerIntPair<const DexString*, 3, IntType> pair{x, 1};

  pair.set_int(2);
  EXPECT_EQ(pair.get_int(), 2);
  EXPECT_EQ(pair.get_pointer(), x);

  pair.set_int(3);
  EXPECT_EQ(pair.get_int(), 3);
  EXPECT_EQ(pair.get_pointer(), x);
}

TEST_F(PointerIntPairTest, SetPointer) {
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");

  PointerIntPair<const DexString*, 3, IntType> pair{nullptr, 1};
  pair.set_pointer(x);

  EXPECT_EQ(pair.get_int(), 1);
  EXPECT_EQ(pair.get_pointer(), x);

  pair.set_pointer(y);
  EXPECT_EQ(pair.get_int(), 1);
  EXPECT_EQ(pair.get_pointer(), y);
}

TEST_F(PointerIntPairTest, SetPointerAndInt) {
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");

  PointerIntPair<const DexString*, 3, IntType> pair{x, 1};
  pair.set_pointer_and_int(y, 2);

  EXPECT_EQ(pair.get_int(), 2);
  EXPECT_EQ(pair.get_pointer(), y);
}

TEST_F(PointerIntPairTest, Comparisons) {
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");

  PointerIntPair<const DexString*, 3, IntType> pair_one{x, 1};
  PointerIntPair<const DexString*, 3, IntType> pair_two{x, 1};

  // equality
  EXPECT_TRUE(pair_one == pair_two);
  EXPECT_FALSE(pair_one != pair_two);

  // different pointer value only
  pair_two.set_pointer(y);
  EXPECT_FALSE(pair_one == pair_two);
  EXPECT_TRUE(pair_one != pair_two);

  // different pointer and int values
  pair_two.set_int(2);
  EXPECT_FALSE(pair_one == pair_two);
  EXPECT_TRUE(pair_one != pair_two);

  // different int value only
  pair_two.set_pointer(x);
  EXPECT_FALSE(pair_one == pair_two);
  EXPECT_TRUE(pair_one != pair_two);
}

TEST_F(PointerIntPairTest, LargeIntegerValues) {
  const auto* x = DexString::make_string("x");

  PointerIntPair<const DexString*, 3, IntType> pair{x, 1};
  // Okay for 3 bits
  pair.set_int(7);
  EXPECT_EQ(pair.get_int(), 7);

  EXPECT_THROW(pair.set_int(8), RedexException);
}

} // namespace marianatrench
