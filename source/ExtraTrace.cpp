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

Json::Value ExtraTrace::to_json() const {
  // NOTE: Keep format in-sync with LocalTaint::to_json() when representing
  // next-hop details. The parser assumes they are the same.
  auto extra_trace = Json::Value(Json::objectValue);
  extra_trace["kind"] = kind_->to_trace_string();
  if (call_kind_.is_origin()) {
    // There are no callees at the origin, so the "origins" below is empty.
    auto origin = LocalTaint::origin_json(position_, OriginSet{});
    if (!origin.empty()) {
      extra_trace["origin"] = origin;
    }
  } else if (call_kind_.is_callsite()) {
    auto call = LocalTaint::next_hop_json(callee_, position_, callee_port_);
    if (!call.empty()) {
      extra_trace["call"] = call;
    }
  }

  return extra_trace;
}

std::ostream& operator<<(std::ostream& out, const ExtraTrace& extra_trace) {
  return out << "ExtraTrace(kind=" << show(extra_trace.kind_)
             << ", position=" << show(extra_trace.position_)
             << ", callee=" << show(extra_trace.callee_)
             << ", callee_port=" << show(extra_trace.callee_port_)
             << ", call_kind=" << extra_trace.call_kind_ << ")";
}

} // namespace marianatrench
