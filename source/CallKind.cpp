/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>

#include <mariana-trench/Assert.h>
#include <mariana-trench/CallKind.h>

namespace marianatrench {

CallKind::CallKind(Encoding encoding) : encoding_(encoding) {}

std::string CallKind::to_trace_string() const {
  std::string call_kind = "";

  if (is_propagation_with_trace()) {
    call_kind = "PropagationWithTrace:";
  }

  if (is_declaration()) {
    call_kind += "Declaration";
  } else if (is_origin()) {
    call_kind += "Origin";
  } else if (is_callsite()) {
    call_kind += "CallSite";
  } else {
    mt_assert(is_propagation_without_trace());
    call_kind += "Propagation";
  }

  return call_kind;
}

std::ostream& operator<<(std::ostream& out, const CallKind& call_kind) {
  return out << call_kind.to_trace_string();
}

CallKind CallKind::propagation_with_trace(CallKind::Encoding kind) {
  mt_assert(
      kind == CallKind::Declaration || kind == CallKind::Origin ||
      kind == CallKind::CallSite);

  return CallKind{CallKind::PropagationWithTrace | kind};
}

CallKind CallKind::decode(Encoding encoding) {
  mt_assert(
      ((encoding & (CallKind::PropagationWithTrace)) !=
       CallKind::PropagationWithTrace) ||
      ((encoding & (CallKind::Propagation)) != CallKind::Propagation));

  return CallKind(encoding);
}

bool CallKind::is_declaration() const {
  return (encoding_ & (~CallKind::PropagationWithTrace)) == Declaration;
}

bool CallKind::is_origin() const {
  return (encoding_ & (~CallKind::PropagationWithTrace)) == Origin;
}

bool CallKind::is_callsite() const {
  return (encoding_ & (~CallKind::PropagationWithTrace)) == CallSite;
}

bool CallKind::is_propagation() const {
  return encoding_ == Propagation ||
      (encoding_ & CallKind::PropagationWithTrace) == PropagationWithTrace;
}

bool CallKind::is_propagation_with_trace() const {
  return (encoding_ & CallKind::PropagationWithTrace) == PropagationWithTrace;
}

bool CallKind::is_propagation_without_trace() const {
  return encoding_ == Propagation;
}

CallKind CallKind::propagate() const {
  if (is_propagation_without_trace()) {
    return *this;
  }

  Encoding encoding{};

  if (is_propagation_with_trace()) {
    encoding |= CallKind::PropagationWithTrace;
  }

  // Propagate the callinfo states.
  if (is_declaration()) {
    encoding |= CallKind::Origin;
  } else if (is_origin()) {
    encoding |= CallKind::CallSite;
  } else if (is_callsite()) {
    encoding |= CallKind::CallSite;
  }

  return CallKind{encoding};
}

} // namespace marianatrench
