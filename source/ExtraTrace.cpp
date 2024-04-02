/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <json/value.h>

#include <Show.h>

#include <mariana-trench/ExtraTrace.h>
#include <mariana-trench/LocalTaint.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

ExtraTrace ExtraTrace::from_json(const Json::Value& value, Context& context) {
  auto call_info = CallInfo::from_json(value, context);
  const auto* kind = Kind::from_json(value, context);
  return ExtraTrace(
      kind,
      call_info.callee(),
      call_info.call_position(),
      call_info.callee_port(),
      call_info.call_kind());
}

Json::Value ExtraTrace::to_json() const {
  auto extra_trace = call_info_.to_json();
  mt_assert(extra_trace.isObject() && !extra_trace.isNull());

  auto kind = kind_->to_json();
  mt_assert(kind.isObject() && !kind.isNull());
  for (const auto& member : kind.getMemberNames()) {
    extra_trace[member] = kind[member];
  }

  return extra_trace;
}

std::ostream& operator<<(std::ostream& out, const ExtraTrace& extra_trace) {
  return out << "ExtraTrace(kind=" << show(extra_trace.kind())
             << ", position=" << show(extra_trace.position())
             << ", callee=" << show(extra_trace.callee())
             << ", callee_port=" << show(extra_trace.callee_port())
             << ", call_kind=" << extra_trace.call_kind() << ")";
}

} // namespace marianatrench
