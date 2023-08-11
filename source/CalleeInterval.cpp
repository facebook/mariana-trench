/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <ostream>

#include <mariana-trench/CalleeInterval.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/TaintConfig.h>

namespace marianatrench {

CalleeInterval::CalleeInterval(
    ClassIntervals::Interval interval,
    bool preserves_type_context)
    : interval_(std::move(interval)),
      preserves_type_context_(preserves_type_context) {}

CalleeInterval::CalleeInterval(const TaintConfig& config)
    : CalleeInterval(config.callee_interval()) {}

CalleeInterval::CalleeInterval(const Frame& frame)
    : CalleeInterval(frame.callee_interval()) {}

Json::Value CalleeInterval::to_json() const {
  auto value = Json::Value(Json::objectValue);
  value["callee_interval"] = ClassIntervals::interval_to_json(interval_);
  value["preserves_type_context"] = Json::Value(preserves_type_context_);
  return value;
}

std::ostream& operator<<(
    std::ostream& out,
    const CalleeInterval& callee_interval) {
  return out << "{" << callee_interval.interval() << ", preserves_type_context="
             << callee_interval.preserves_type_context() << "}";
}

} // namespace marianatrench
