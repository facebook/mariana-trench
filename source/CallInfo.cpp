/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/CallInfo.h>

namespace marianatrench {

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
