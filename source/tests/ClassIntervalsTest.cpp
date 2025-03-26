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
  context.options = std::make_unique<Options>(
      /* models_path */ std::vector<std::string>{},
      /* field_models_path */ std::vector<std::string>{},
      /* literal_models_path */ std::vector<std::string>{},
      /* rules_path */ std::vector<std::string>{},
      /* lifecycles_path */ std::vector<std::string>{},
      /* shims_path */ std::vector<std::string>{},
      /* graphql_metadata_paths */ std::string{},
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ true,
      /* skip_source_indexing */ true,
      /* skip_analysis */ false,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generator_search_paths */ std::vector<std::string>{},
      /* emit_all_via_cast_features */ false,
      /* remove_unreachable_code */ false);
  CachedModelsContext cached_models_context(context, *context.options);

  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};
  context.options = test::make_default_options();
  context.class_intervals = std::make_unique<ClassIntervals>(
      *context.options, context.stores, cached_models_context);
  return context;
}

} // anonymous namespace

TEST_F(ClassIntervalsTest, IntervalComputation) {
  Scope scope;

  // Construct simple class hierarchy, rooted in BaseA and BaseB.

  // BaseA [0,5] -> DerivedA1 [1,2]
  //             -> DerivedA2 [3,4]
  const auto* a = marianatrench::redex::create_class(scope, "LBaseA;");
  const auto* a1 =
      marianatrench::redex::create_class(scope, "LDerivedA1;", a->get_type());
  const auto* a2 =
      marianatrench::redex::create_class(scope, "LDerivedA2;", a->get_type());

  // BaseB [6,11] -> DerivedB1 [7,10] -> DerivedB1_1 [8,9]
  const auto* b = marianatrench::redex::create_class(scope, "LBaseB;");
  const auto* b1 =
      marianatrench::redex::create_class(scope, "LDerivedB1;", b->get_type());
  const auto* b1_1 = marianatrench::redex::create_class(
      scope, "LDerivedB1_1;", b1->get_type());

  auto context = test_context(scope);

  auto interval_a = context.class_intervals->get_interval(a->get_type());
  EXPECT_EQ(ClassIntervals::Interval::finite(0, 5), interval_a);

  auto interval_a1 = context.class_intervals->get_interval(a1->get_type());
  EXPECT_EQ(ClassIntervals::Interval::finite(1, 2), interval_a1);

  auto interval_a2 = context.class_intervals->get_interval(a2->get_type());
  EXPECT_EQ(ClassIntervals::Interval::finite(3, 4), interval_a2);

  auto interval_b = context.class_intervals->get_interval(b->get_type());
  EXPECT_EQ(ClassIntervals::Interval::finite(6, 11), interval_b);

  auto interval_b1 = context.class_intervals->get_interval(b1->get_type());
  EXPECT_EQ(ClassIntervals::Interval::finite(7, 10), interval_b1);

  auto interval_b1_1 = context.class_intervals->get_interval(b1_1->get_type());
  EXPECT_EQ(ClassIntervals::Interval::finite(8, 9), interval_b1_1);

  auto interval_object =
      context.class_intervals->get_interval(type::java_lang_Object());
  EXPECT_EQ(ClassIntervals::Interval::top(), interval_object);
}

TEST_F(ClassIntervalsTest, IntervalSerializationDeserialization) {
  Scope scope;

  // Construct simple class hierarchy, rooted in BaseA and BaseB.

  // BaseA [0,5] -> DerivedA1 [1,2]
  //             -> DerivedA2 [3,4]
  const auto* a = marianatrench::redex::create_class(scope, "LBaseA;");
  marianatrench::redex::create_class(scope, "LDerivedA1;", a->get_type());
  marianatrench::redex::create_class(scope, "LDerivedA2;", a->get_type());

  // BaseB [6,11] -> DerivedB1 [7,10] -> DerivedB1_1 [8,9]
  const auto* b = marianatrench::redex::create_class(scope, "LBaseB;");
  const auto* b1 =
      marianatrench::redex::create_class(scope, "LDerivedB1;", b->get_type());
  marianatrench::redex::create_class(scope, "LDerivedB1_1;", b1->get_type());

  // This constructs the class intervals from the scope.
  auto context = test_context(scope);

  auto intervals_json = context.class_intervals->to_json();
  auto intervals_map = ClassIntervals::from_json(intervals_json);

  // Every class should have an interval. Note that java.lang.Object which is
  // not stored in the ClassIntervals, is also absent from Scope in this test
  // environment.
  EXPECT_EQ(intervals_map.size(), scope.size());

  // Verifies that class intervals are the same as the original after
  // deserialization.
  for (const auto* klass : scope) {
    const auto* klass_type = klass->get_type();
    EXPECT_EQ(
        context.class_intervals->get_interval(klass_type),
        intervals_map.find(klass_type)->second);
  }
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
