/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>
#include <TypeUtil.h>

#include <json/value.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

namespace {

// Sets the class interval for `current_node` while performing a DFS on it.
void dfs_on_hierarchy(
    const ClassHierarchy& class_hierarchy,
    const DexType* current_node,
    std::uint32_t& dfs_order,
    std::unordered_map<const DexType*, ClassIntervals::Interval>& result) {
  auto lower_bound = dfs_order;

  auto children = class_hierarchy.find(current_node);
  mt_assert(children != class_hierarchy.end());

  for (const auto* child : children->second) {
    ++dfs_order;
    mt_assert(dfs_order > 0); // Ensure no overflows.
    dfs_on_hierarchy(class_hierarchy, child, dfs_order, result);
  }

  ++dfs_order;
  mt_assert(dfs_order > 0); // Ensure no overflows.

  // Each node should only be visited once since multiple inheritance is not
  // supported by Java/Kotlin.
  mt_assert(result.find(current_node) == result.end());
  auto interval = ClassIntervals::Interval::finite(lower_bound, dfs_order);
  result.emplace(current_node, interval);
}

} // namespace

ClassIntervals::ClassIntervals(
    const Options& options,
    const DexStoresVector& stores)
    : top_(Interval::top()) {
  ClassHierarchy class_hierarchy;
  for (const auto& scope : DexStoreClassesIterator(stores)) {
    auto store_hierarchy = build_type_hierarchy(scope);
    for (const auto& [parent, children] : store_hierarchy) {
      class_hierarchy[parent].insert(children.begin(), children.end());
    }
  }

  // Assuming the code is known, all classes will be rooted in java.lang.Object.
  // Optimization: Divide the single hierarchy into multiple, with roots at
  // direct children of Object, then compute in parallel. Need to make sure
  // dfs_order does not intersect between different trees.
  const auto* root = type::java_lang_Object();

  std::uint32_t dfs_order = MIN_INTERVAL;
  dfs_on_hierarchy(class_hierarchy, root, dfs_order, class_intervals_);

  if (options.dump_class_intervals()) {
    auto class_intervals_path = options.class_intervals_output_path();
    LOG(1, "Writing class intervals to `{}`", class_intervals_path.native());
    JsonWriter::write_json_file(class_intervals_path, to_json());

    // Dumping class intervals is test-only, perform additional, otherwise
    // unnecessary/expensive validation here.
    for (const auto& scope : DexStoreClassesIterator(stores)) {
      for (const auto& klass : scope) {
        if (!is_interface(klass) &&
            class_intervals_.find(klass->get_type()) ==
                class_intervals_.end()) {
          // Might happen if not everything was rooted in Object.
          WARNING(1, "Did not compute interval for `{}`.", show(klass));
        }
      }
    }
  }
}

const ClassIntervals::Interval& ClassIntervals::get_interval(
    const DexType* type) const {
  auto interval = class_intervals_.find(type);
  if (interval == class_intervals_.end()) {
    // Type not found. Use top to represent the broadest possible type.
    return top_;
  }

  return interval->second;
}

Json::Value ClassIntervals::interval_to_json(const Interval& interval) {
  auto interval_json = Json::Value(Json::arrayValue);
  if (interval.is_bottom()) {
    // Empty array for bottom interval.
    return interval_json;
  }

  // Use the int64 constructor. This allows comparison against a Json::Value
  // object returned from parsing a JSON string. Otherwise, we could end up
  // comparing a Json::UInt type against a Json::Int type and fail equality
  // check even for the same integer value.
  interval_json.append(
      Json::Value(static_cast<int64_t>(interval.lower_bound())));
  interval_json.append(
      Json::Value(static_cast<int64_t>(interval.upper_bound())));

  return interval_json;
}

ClassIntervals::Interval ClassIntervals::interval_from_json(
    const Json::Value& value) {
  auto bounds = JsonValidation::null_or_array(value);
  if (bounds.empty()) {
    return Interval::bottom();
  }

  if (bounds.size() != 2) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, "array of size 2 for class interval");
  }

  // to_json() converts it to int64_t for Json comparison purposes, but the
  // underlying type supports only uint32, so it is parsed as that.
  auto lower_bound = JsonValidation::unsigned_integer(bounds[0]);
  auto upper_bound = JsonValidation::unsigned_integer(bounds[1]);

  if (lower_bound == Interval::MIN && upper_bound == Interval::MAX) {
    return Interval::top();
  } else if (lower_bound == Interval::MIN) {
    return Interval::bounded_above(upper_bound);
  } else if (upper_bound == Interval::MAX) {
    return Interval::bounded_below(lower_bound);
  }

  return Interval::finite(lower_bound, upper_bound);
}

Json::Value ClassIntervals::to_json() const {
  auto output = Json::Value(Json::objectValue);
  for (auto [klass, interval] : class_intervals_) {
    output[show(klass)] = interval_to_json(interval);
  }
  return output;
}

} // namespace marianatrench
