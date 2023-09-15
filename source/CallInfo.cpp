/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>

#include <mariana-trench/Assert.h>
#include <mariana-trench/CallInfo.h>

namespace marianatrench {

CallInfo::CallInfo(KindEncoding call_infos) : call_infos_(call_infos) {}

std::string CallInfo::to_trace_string() const {
  std::string call_info = "";

  if (is_propagation_with_trace()) {
    call_info = "PropagationWithTrace:";
  }

  if (is_declaration()) {
    call_info += "Declaration";
  } else if (is_origin()) {
    call_info += "Origin";
  } else if (is_callsite()) {
    call_info += "CallSite";
  } else {
    mt_assert(is_propagation_without_trace());
    call_info += "Propagation";
  }

  return call_info;
}

std::ostream& operator<<(std::ostream& out, const CallInfo& call_info) {
  return out << call_info.to_trace_string();
}

CallInfo CallInfo::declaration() {
  return CallInfo{CallInfo::Declaration};
}

CallInfo CallInfo::origin() {
  return CallInfo{CallInfo::Origin};
}

CallInfo CallInfo::callsite() {
  return CallInfo{CallInfo::CallSite};
}

CallInfo CallInfo::propagation() {
  return CallInfo{CallInfo::Propagation};
}

CallInfo CallInfo::propagation_with_trace(CallInfo::KindEncoding kind) {
  mt_assert(
      kind == CallInfo::Declaration || kind == CallInfo::Origin ||
      kind == CallInfo::CallSite);

  return CallInfo{CallInfo::PropagationWithTrace | kind};
}

bool CallInfo::is_declaration() const {
  return (call_infos_ & (~CallInfo::PropagationWithTrace)) == Declaration;
}

bool CallInfo::is_origin() const {
  return (call_infos_ & (~CallInfo::PropagationWithTrace)) == Origin;
}

bool CallInfo::is_callsite() const {
  return (call_infos_ & (~CallInfo::PropagationWithTrace)) == CallSite;
}

bool CallInfo::is_propagation() const {
  return call_infos_ == Propagation ||
      (call_infos_ & CallInfo::PropagationWithTrace) == PropagationWithTrace;
}

bool CallInfo::is_propagation_with_trace() const {
  return (call_infos_ & CallInfo::PropagationWithTrace) == PropagationWithTrace;
}

bool CallInfo::is_propagation_without_trace() const {
  return call_infos_ == Propagation;
}

CallInfo CallInfo::propagate() const {
  if (is_propagation_without_trace()) {
    return *this;
  }

  KindEncoding call_infos{};

  if (is_propagation_with_trace()) {
    call_infos |= CallInfo::PropagationWithTrace;
  }

  // Propagate the callinfo states.
  if (is_declaration()) {
    call_infos |= CallInfo::Origin;
  } else if (is_origin()) {
    call_infos |= CallInfo::CallSite;
  } else if (is_callsite()) {
    call_infos |= CallInfo::CallSite;
  }

  return CallInfo{call_infos};
}

} // namespace marianatrench
