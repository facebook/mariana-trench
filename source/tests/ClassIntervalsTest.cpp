/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <DexStore.h>

#include <mariana-trench/CallClassIntervalContext.h>
#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class ClassIntervalsTest : public test::Test {};

Context test_context(const Scope& scope) {
  Context context;
  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};
  context.options = test::make_default_options();
  context.class_intervals =
      std::make_unique<ClassIntervals>(*context.options, context.stores);
  return context;
}

} // anonymous namespace

TEST_F(ClassIntervalsTest, IntervalComputation) {
  Scope scope;

  // Construct simple class hierarchy, rooted in BaseA and BaseB.
  // Note that Object() is still the root of everything.

  // BaseA -> DerivedA1
  //       -> DerivedA2
  const auto* a = marianatrench::redex::create_class(scope, "LBaseA;");
  const auto* a1 =
      marianatrench::redex::create_class(scope, "LDerivedA1;", a->get_type());
  const auto* a2 =
      marianatrench::redex::create_class(scope, "LDerivedA2;", a->get_type());

  // BaseB -> DerivedB1 -> DerivedB1_1
  const auto* b = marianatrench::redex::create_class(scope, "LBaseB;");
  const auto* b1 =
      marianatrench::redex::create_class(scope, "LDerivedB1;", b->get_type());
  const auto* b1_1 = marianatrench::redex::create_class(
      scope, "LDerivedB1_1;", b1->get_type());

  auto context = test_context(scope);

  auto interval_a = context.class_intervals->get_interval(a->get_type());
  EXPECT_EQ(ClassIntervals::Interval::finite(1, 6), interval_a);

  auto interval_a1 = context.class_intervals->get_interval(a1->get_type());
  EXPECT_EQ(ClassIntervals::Interval::finite(2, 3), interval_a1);

  auto interval_a2 = context.class_intervals->get_interval(a2->get_type());
  EXPECT_EQ(ClassIntervals::Interval::finite(4, 5), interval_a2);

  auto interval_b = context.class_intervals->get_interval(b->get_type());
  EXPECT_EQ(ClassIntervals::Interval::finite(7, 12), interval_b);

  auto interval_b1 = context.class_intervals->get_interval(b1->get_type());
  EXPECT_EQ(ClassIntervals::Interval::finite(8, 11), interval_b1);

  auto interval_b1_1 = context.class_intervals->get_interval(b1_1->get_type());
  EXPECT_EQ(ClassIntervals::Interval::finite(9, 10), interval_b1_1);

  auto interval_object =
      context.class_intervals->get_interval(type::java_lang_Object());
  EXPECT_EQ(ClassIntervals::Interval::finite(0, 13), interval_object);
}

TEST_F(ClassIntervalsTest, ClassIntervalSerializationDeserialization) {
  {
    auto interval = ClassIntervals::Interval::bottom();
    auto interval_json = ClassIntervals::interval_to_json(interval);
    EXPECT_EQ(ClassIntervals::interval_from_json(interval_json), interval);
  }

  {
    auto interval = ClassIntervals::Interval::top();
    auto interval_json = ClassIntervals::interval_to_json(interval);
    EXPECT_EQ(ClassIntervals::interval_from_json(interval_json), interval);
  }

  {
    auto interval = ClassIntervals::Interval::bounded_below(10);
    auto interval_json = ClassIntervals::interval_to_json(interval);
    EXPECT_EQ(ClassIntervals::interval_from_json(interval_json), interval);
  }

  {
    auto interval = ClassIntervals::Interval::bounded_above(10);
    auto interval_json = ClassIntervals::interval_to_json(interval);
    EXPECT_EQ(ClassIntervals::interval_from_json(interval_json), interval);
  }

  {
    auto interval = ClassIntervals::Interval::finite(1, 10);
    auto interval_json = ClassIntervals::interval_to_json(interval);
    EXPECT_EQ(ClassIntervals::interval_from_json(interval_json), interval);
  }
}

TEST_F(ClassIntervalsTest, CallClassIntervalSerializationDeserialization) {
  {
    auto interval_context = CallClassIntervalContext();
    EXPECT_EQ(
        CallClassIntervalContext::from_json(interval_context.to_json()),
        interval_context);
  }

  {
    auto interval_context = CallClassIntervalContext(
        ClassIntervals::Interval::finite(1, 10),
        /* preserves_type_context */ true);
    EXPECT_EQ(
        CallClassIntervalContext::from_json(interval_context.to_json()),
        interval_context);
  }
}
