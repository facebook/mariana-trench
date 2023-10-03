/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Assert.h>
#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/Hash.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

class TaintConfig;
class Frame;

/**
 * Represents the class interval of a callee in `Taint`.
 *
 * callee_interval_:
 *   Represents the class interval of the method based on the
 *   receiver's type.
 * preserves_type_context_:
 *   True iff the callee was called with `this.` (i.e. the method call's
 *   receiver has the same type as the caller's class).
 */
class CallClassIntervalContext {
 public:
  CallClassIntervalContext()
      : callee_interval_(ClassIntervals::Interval::top()),
        preserves_type_context_(false) {
    // Default constructor is expected to produce a "default" interval.
    mt_assert(is_default());
  }

  explicit CallClassIntervalContext(
      ClassIntervals::Interval interval,
      bool preserves_type_context);

  explicit CallClassIntervalContext(const TaintConfig& config);

  explicit CallClassIntervalContext(const Frame& frame);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CallClassIntervalContext)

  bool operator==(const CallClassIntervalContext& other) const {
    return callee_interval_ == other.callee_interval_ &&
        preserves_type_context_ == other.preserves_type_context_;
  }

  bool is_default() const {
    return callee_interval_.is_top() && !preserves_type_context_;
  }

  const ClassIntervals::Interval& callee_interval() const {
    return callee_interval_;
  }

  bool preserves_type_context() const {
    return preserves_type_context_;
  }

  Json::Value to_json() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const CallClassIntervalContext& interval);

 private:
  ClassIntervals::Interval callee_interval_;
  bool preserves_type_context_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::CallClassIntervalContext> {
  std::size_t operator()(const marianatrench::CallClassIntervalContext&
                             class_interval_context) const {
    std::size_t seed = 0;
    boost::hash_combine(
        seed,
        std::hash<marianatrench::ClassIntervals::Interval>()(
            class_interval_context.callee_interval()));
    boost::hash_combine(
        seed,
        std::hash<bool>()(class_interval_context.preserves_type_context()));
    return seed;
  }
};
