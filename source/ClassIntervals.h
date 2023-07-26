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
 public:
  struct Interval {
    /**
     * Defaults to the equivalent of an open interval: (-inf,+inf) which
     * represents all classes.
     */
    explicit Interval()
        : lower_bound(std::numeric_limits<std::uint32_t>::min()),
          upper_bound(std::numeric_limits<std::uint32_t>::max()) {}

    explicit Interval(std::uint32_t lower_bound, std::uint32_t upper_bound)
        : lower_bound(lower_bound), upper_bound(upper_bound) {}

    static Interval max_interval() {
      return Interval();
    }

    INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Interval)

    bool operator==(const Interval& other) const;

    /**
     * Returns true if this interval contains `other`.
     * Semantically, returns true if the class represented by the interval
     * `other` is derived from the class represented by this interval.
     */
    bool contains(const Interval& other) const;

    friend std::ostream& operator<<(std::ostream&, const Interval&);

    std::uint32_t lower_bound;
    std::uint32_t upper_bound;
  };

 public:
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

  Json::Value to_json() const;

 private:
  const Interval open_interval_;

  std::unordered_map<const DexType*, Interval> class_intervals_;
};

} // namespace marianatrench
