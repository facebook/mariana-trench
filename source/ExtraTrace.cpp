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
  auto extra_trace = call_info_.to_json(/* origins */ {});
  mt_assert(extra_trace.isObject() && !extra_trace.isNull());
  extra_trace["kind"] = kind_->to_trace_string();
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
