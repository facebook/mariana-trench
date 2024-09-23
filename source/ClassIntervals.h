/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <limits>
#include <unordered_map>

#include <json/json.h>

#include <sparta/IntervalDomain.h>

#include <ClassHierarchy.h>
#include <DexStore.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Options.h>

namespace marianatrench {

/**
 * Class intervals are used to describe a hierarchy as follows:
 * - An interval is a [lower bound, upper bound] pair.
 * - interval(Derived) is contained within interval(Base).
 * - interval(SiblingA) is disjoint from interval(SiblingB).
 * - Intervals are computed using the DFS order. The lower bound is the first
 *   time a node is visited and the upper bound is the last time the node is
 *   visited. The Base is always visited first before its Derived classes and
 *   last visited after its Derived classes in the DFS order hence giving the
 *   properties described above.
 *
 * Example:
 *   Base [0,7]
 *   /            \
 * Derived1[1,4]   Derived2 [5,6]
 *  |
 * Derived2[2,3]
 */
class ClassIntervals final {
 private:
  // NOTE: std::numeric_limits<std::int32_t>::min() is internally used by
  // sparta::IntervalDomain to represent an interval that is unbounded below.
  // Class intervals are bounded, and by convention, represented by non-negative
  // integers.
  static constexpr std::int32_t MIN_INTERVAL = 0;

 public:
  using Interval = sparta::IntervalDomain<std::int32_t>;

  explicit ClassIntervals(
      const Options& options,
      const DexStoresVector& stores);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ClassIntervals)

  /**
   * Returns the most precisely known interval of the given type.
   * This is generally the computed type, but can be the open interval, such as
   * when class interval computation is disabled, or for non-class types.
   */
  const Interval& get_interval(const DexType* type) const;

  static Json::Value interval_to_json(const Interval& interval);
  static Interval interval_from_json(const Json::Value& value);

  Json::Value to_json() const;

 private:
  const Interval top_;

  std::unordered_map<const DexType*, Interval> class_intervals_;
};

} // namespace marianatrench
