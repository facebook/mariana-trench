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

CallInfo::CallInfo(CallInfoFlags call_infos) : call_infos_(call_infos) {
  static const CallInfo::CallInfoFlags k_mutually_exclusive_call_info =
      CallInfo::Kind::Declaration | CallInfo::Kind::Origin |
      CallInfo::Kind::CallSite;

  if (call_infos.test(CallInfo::Kind::Propagation)) {
    // Check that none of the mutually exclusive call info are set.
    mt_assert((call_infos & k_mutually_exclusive_call_info).empty());
  } else {
    // Check that only 1 of the mutually exclusive call info are set.
    auto check = (call_infos & k_mutually_exclusive_call_info);
    mt_assert(check.has_single_bit());
  }
}

std::string CallInfo::to_trace_string() const {
  std::string call_info = "";

  if (call_infos_.test(CallInfo::Kind::PropagationWithTrace)) {
    call_info = "PropagationWithTrace:";
  }

  if (call_infos_.test(CallInfo::Kind::Declaration)) {
    call_info += "Declaration";
  } else if (call_infos_.test(CallInfo::Kind::Origin)) {
    call_info += "Origin";
  } else if (call_infos_.test(CallInfo::Kind::CallSite)) {
    call_info += "CallSite";
  } else {
    mt_assert(call_infos_.test(CallInfo::Kind::Propagation));
    call_info += "Propagation";
  }

  return call_info;
}

std::ostream& operator<<(std::ostream& out, const CallInfo& call_info) {
  return out << call_info.to_trace_string();
}

CallInfo CallInfo::propagate() const {
  CallInfoFlags call_infos{};

  if (call_infos_.test(CallInfo::Kind::Declaration)) {
    call_infos |= CallInfo::Kind::Origin;
  } else if (call_infos_.test(CallInfo::Kind::Origin)) {
    call_infos |= CallInfo::Kind::CallSite;
  } else if (call_infos_.test(CallInfo::Kind::CallSite)) {
    call_infos |= CallInfo::Kind::CallSite;
  }

  if (call_infos_.test(CallInfo::Kind::Propagation)) {
    call_infos |= CallInfo::Kind::Propagation;
  } else if (call_infos_.test(CallInfo::Kind::PropagationWithTrace)) {
    call_infos |= CallInfo::Kind::PropagationWithTrace;
  }

  return CallInfo{call_infos};
}

} // namespace marianatrench
