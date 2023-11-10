/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/CallInfo.h>

namespace marianatrench {

Json::Value CallInfo::to_json() const {
  auto result = Json::Value(Json::objectValue);

  // The next hop is indicated by a CallInfo object.
  //
  // When call_kind = origin, this is a leaf taint and there is no next hop.
  // Examples of when this is the case:
  // - Calling into a method(/frame) where a source/sink is defined, i.e.
  //   declaration frame.
  // - Return sinks and parameter sources. There is no callee for these, but
  //   the position points to the return instruction/parameter.

  // TODO(T163918472): For call_kind == origin, ensure the following
  // invariants: callee == nullptr && callee_port.is_leaf()
  auto call_info = Json::Value(Json::objectValue);
  call_info["call_kind"] = call_kind().to_trace_string();
  const auto* callee = this->callee();
  if (callee != nullptr) {
    call_info["resolves_to"] = callee->to_json();
  }
  if (call_position_ != nullptr) {
    call_info["position"] = call_position_->to_json();
  }
  if (callee_port_ != nullptr && !callee_port_->root().is_leaf()) {
    call_info["port"] = callee_port_->to_json();
  }

  result["call_info"] = call_info;
  return result;
}

bool operator==(const CallInfo& self, const CallInfo& other) {
  return self.method_call_kind_ == other.method_call_kind_ &&
      self.callee_port_ == other.callee_port_ &&
      self.call_position_ == other.call_position_;
}

bool operator<(const CallInfo& self, const CallInfo& other) {
  // Lexicographic comparison.
  return std::make_tuple(
             self.method_call_kind_.encode(),
             self.callee_port_,
             self.call_position_) <
      std::make_tuple(
             other.method_call_kind_.encode(),
             other.callee_port_,
             other.call_position_);
}

std::ostream& operator<<(std::ostream& out, const CallInfo& call_info) {
  if (call_info.is_default()) {
    return out << "CallInfo()";
  }

  return out << "CallInfo(callee=`" << show(call_info.callee())
             << "`, call_kind=" << call_info.call_kind()
             << ", callee_port=" << show(call_info.callee_port())
             << ", call_position=" << show(call_info.call_position()) << ")";
}

} // namespace marianatrench
