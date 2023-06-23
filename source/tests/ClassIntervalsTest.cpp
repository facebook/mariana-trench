/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <DexStore.h>

#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class ClassIntervalsTest : public test::Test {};

Context test_context(const Scope& scope) {
  Context context;
  context.options = std::make_unique<Options>(
      /* models_path */ std::vector<std::string>{},
      /* field_models_path */ std::vector<std::string>{},
      /* rules_path */ std::vector<std::string>{},
      /* lifecycles_path */ std::vector<std::string>{},
      /* shims_path */ std::vector<std::string>{},
      /* graphql_metadata_paths */ std::string{},
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ false,
      /* skip_source_indexing */ true,
      /* skip_analysis */ true,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generator_search_paths */ std::vector<std::string>{},
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ false,
      /* source_root_directory */ ".",
      /* enable_cross_component_analysis */ false,
      /* enable_class_intervals */ true);
  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};
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
  const auto* a = redex::create_class(scope, "LBaseA;");
  const auto* a1 = redex::create_class(scope, "LDerivedA1;", a->get_type());
  const auto* a2 = redex::create_class(scope, "LDerivedA2;", a->get_type());

  // BaseB -> DerivedB1 -> DerivedB1_1
  const auto* b = redex::create_class(scope, "LBaseB;");
  const auto* b1 = redex::create_class(scope, "LDerivedB1;", b->get_type());
  const auto* b1_1 =
      redex::create_class(scope, "LDerivedB1_1;", b1->get_type());

  auto context = test_context(scope);

  auto interval_a = context.class_intervals->get_interval(a->get_type());
  EXPECT_EQ(ClassIntervals::Interval(1, 6), interval_a);
  EXPECT_TRUE(ClassIntervals::Interval().contains(interval_a));
  EXPECT_FALSE(interval_a.contains(ClassIntervals::Interval()));

  auto interval_a1 = context.class_intervals->get_interval(a1->get_type());
  EXPECT_EQ(ClassIntervals::Interval(2, 3), interval_a1);
  EXPECT_TRUE(ClassIntervals::Interval().contains(interval_a1));
  EXPECT_FALSE(interval_a1.contains(ClassIntervals::Interval()));
  EXPECT_FALSE(interval_a1.contains(interval_a));
  EXPECT_TRUE(interval_a.contains(interval_a1));

  auto interval_a2 = context.class_intervals->get_interval(a2->get_type());
  EXPECT_EQ(ClassIntervals::Interval(4, 5), interval_a2);
  EXPECT_TRUE(ClassIntervals::Interval().contains(interval_a2));
  EXPECT_FALSE(interval_a2.contains(ClassIntervals::Interval()));
  EXPECT_FALSE(interval_a2.contains(interval_a));
  EXPECT_TRUE(interval_a.contains(interval_a2));
  EXPECT_FALSE(interval_a2.contains(interval_a1));
  EXPECT_FALSE(interval_a1.contains(interval_a2));

  auto interval_b = context.class_intervals->get_interval(b->get_type());
  EXPECT_EQ(ClassIntervals::Interval(7, 12), interval_b);
  EXPECT_TRUE(ClassIntervals::Interval().contains(interval_b));
  EXPECT_FALSE(interval_b.contains(ClassIntervals::Interval()));
  EXPECT_FALSE(interval_b.contains(interval_a));
  EXPECT_FALSE(interval_a.contains(interval_b));

  auto interval_b1 = context.class_intervals->get_interval(b1->get_type());
  EXPECT_EQ(ClassIntervals::Interval(8, 11), interval_b1);
  EXPECT_TRUE(ClassIntervals::Interval().contains(interval_b1));
  EXPECT_FALSE(interval_b1.contains(ClassIntervals::Interval()));
  EXPECT_FALSE(interval_b1.contains(interval_b));
  EXPECT_TRUE(interval_b.contains(interval_b1));

  auto interval_b1_1 = context.class_intervals->get_interval(b1_1->get_type());
  EXPECT_EQ(ClassIntervals::Interval(9, 10), interval_b1_1);
  EXPECT_TRUE(ClassIntervals::Interval().contains(interval_b1_1));
  EXPECT_FALSE(interval_b1_1.contains(ClassIntervals::Interval()));
  EXPECT_FALSE(interval_b1_1.contains(interval_b1));
  EXPECT_TRUE(interval_b.contains(interval_b1_1));
  EXPECT_TRUE(interval_b1.contains(interval_b1_1));

  auto interval_object =
      context.class_intervals->get_interval(type::java_lang_Object());
  EXPECT_EQ(ClassIntervals::Interval(0, 13), interval_object);
  EXPECT_TRUE(interval_object.contains(interval_a));
  EXPECT_TRUE(interval_object.contains(interval_a1));
  EXPECT_TRUE(interval_object.contains(interval_a2));
  EXPECT_TRUE(interval_object.contains(interval_b));
  EXPECT_TRUE(interval_object.contains(interval_b1));
  EXPECT_TRUE(interval_object.contains(interval_b1_1));
  EXPECT_FALSE(interval_a.contains(interval_object));
  EXPECT_FALSE(interval_a1.contains(interval_object));
  EXPECT_FALSE(interval_a2.contains(interval_object));
  EXPECT_FALSE(interval_b.contains(interval_object));
  EXPECT_FALSE(interval_b1.contains(interval_object));
  EXPECT_FALSE(interval_b1_1.contains(interval_object));
}

TEST_F(ClassIntervalsTest, Contains) {
  // Verifies:
  // - Interval contains itself.
  // - Open interval contains everything.
  // - Nothing contains open interval, except open interval.
  // - Disjoint or partially intersecting intervals do not contain each other.
  // NOTE: A.contains(B) && B.contains(A) => A == B

  ClassIntervals::Interval open_interval;
  EXPECT_TRUE(open_interval.contains(open_interval));

  ClassIntervals::Interval interval1_1(1, 1);
  EXPECT_TRUE(interval1_1.contains(interval1_1));
  EXPECT_TRUE(open_interval.contains(interval1_1));
  EXPECT_FALSE(interval1_1.contains(open_interval));

  // Disjoint interval
  ClassIntervals::Interval interval3_4(3, 4);
  EXPECT_FALSE(interval3_4.contains(interval1_1));
  EXPECT_FALSE(interval1_1.contains(interval3_4));

  // Partially intersecting interval
  ClassIntervals::Interval interval2_3(2, 3);
  EXPECT_FALSE(interval2_3.contains(interval3_4));
  EXPECT_FALSE(interval3_4.contains(interval2_3));

  // Larger, containing interval
  ClassIntervals::Interval interval1_4(1, 4);
  EXPECT_TRUE(interval1_4.contains(interval1_1));
  EXPECT_TRUE(interval1_4.contains(interval3_4));
  EXPECT_TRUE(interval1_4.contains(interval2_3));
  EXPECT_FALSE(interval1_1.contains(interval1_4));
  EXPECT_FALSE(interval3_4.contains(interval1_4));
  EXPECT_FALSE(interval2_3.contains(interval1_4));
}
