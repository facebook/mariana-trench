/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Assert.h>
#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

class TaintConfig;
class Frame;

/**
 * Represents the class interval of a callee in `Taint`.
 *
 * interval_:
 *   Represents the class interval of the method based on the
 *   receiver's type.
 * preserves_type_context_:
 *   True iff the callee was called with `this.` (i.e. the method call's
 *   receiver has the same type as the caller's class).
 */
class CalleeInterval {
 public:
  CalleeInterval()
      : interval_(ClassIntervals::Interval::top()),
        preserves_type_context_(false) {
    // Default constructor is expected to produce a "default" interval.
    mt_assert(is_default());
  }

  explicit CalleeInterval(
      ClassIntervals::Interval interval,
      bool preserves_type_context);

  explicit CalleeInterval(const TaintConfig& config);

  explicit CalleeInterval(const Frame& frame);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CalleeInterval)

  bool operator==(const CalleeInterval& other) const {
    return interval_ == other.interval_ &&
        preserves_type_context_ == other.preserves_type_context_;
  }

  bool is_default() const {
    return interval_.is_top() && !preserves_type_context_;
  }

  const ClassIntervals::Interval& interval() const {
    return interval_;
  }

  bool preserves_type_context() const {
    return preserves_type_context_;
  }

  Json::Value to_json() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const CalleeInterval& interval);

 private:
  ClassIntervals::Interval interval_;
  bool preserves_type_context_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::CalleeInterval> {
  std::size_t operator()(
      const marianatrench::CalleeInterval& callee_interval) const {
    std::size_t seed = 0;
    boost::hash_combine(
        seed,
        std::hash<marianatrench::ClassIntervals::Interval>()(
            callee_interval.interval()));
    boost::hash_combine(
        seed, std::hash<bool>()(callee_interval.preserves_type_context()));
    return seed;
  }
};
