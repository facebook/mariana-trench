/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <json/value.h>

#include <Show.h>

#include <mariana-trench/ExtraTrace.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

Json::Value ExtraTrace::to_json() const {
  auto extra_trace = Json::Value(Json::objectValue);
  extra_trace["kind"] = kind_->to_trace_string();
  if (call_kind_.is_origin()) {
    if (position_ != nullptr) {
      auto origin = Json::Value(Json::objectValue);
      origin["position"] = position_->to_json();
      if (callee_ != nullptr) {
        origin["method"] = callee_->to_json();
      }
      extra_trace["origin"] = origin;
    }
  } else if (call_kind_.is_callsite()) {
    auto call = Json::Value(Json::objectValue);
    if (callee_ != nullptr) {
      call["resolves_to"] = callee_->to_json();
    }
    if (position_ != nullptr) {
      call["position"] = position_->to_json();
    }
    if (!callee_port_->root().is_leaf()) {
      call["port"] = callee_port_->to_json();
    }
    extra_trace["call"] = call;
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
