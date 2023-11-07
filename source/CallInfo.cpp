/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/CallInfo.h>

namespace marianatrench {

namespace {

/**
 * Returns a JSON representation of the next hop. Used only when there is a
 * valid next hop. Use `origin_json()` otherwise.
 */
Json::Value next_hop_json(
    const Method* MT_NULLABLE callee,
    const Position* MT_NULLABLE callee_position,
    const AccessPath* MT_NULLABLE callee_port) {
  auto call = Json::Value(Json::objectValue);
  if (callee != nullptr) {
    call["resolves_to"] = callee->to_json();
  }
  if (callee_position != nullptr) {
    call["position"] = callee_position->to_json();
  }
  if (callee_port != nullptr && !callee_port->root().is_leaf()) {
    call["port"] = callee_port->to_json();
  }
  return call;
}

/**
 * Returns a JSON representation of the origin. Used when a call/frame is
 * terminal, a.k.a. a leaf call, or the origin of the taint. If there is a
 * next hop, use `next_hop_json()`.
 */
Json::Value origin_json(const Position* MT_NULLABLE callee_position) {
  auto origin = Json::Value(Json::objectValue);
  if (callee_position != nullptr) {
    origin["position"] = callee_position->to_json();
  }
  return origin;
}

} // namespace

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

  const Method* callee = this->callee();
  CallKind call_kind = this->call_kind();
  const AccessPath* callee_port = this->callee_port();
  const Position* call_position = this->call_position();

  if (call_kind.is_origin()) {
    // Since we don't emit calls for origins, we need to provide the origin
    // location for proper visualisation.

    auto origin = origin_json(call_position);

    // TODO(T163918472): Remove this in favor of "leaves" after parser is
    // updated. New format should work for CRTEX as well. Callee should
    // always be nullptr at origins.
    if (callee != nullptr) {
      origin["method"] = callee->to_json();
    }

    // TODO(T163918472): Remove this in favor of "leaves" after parser is
    // updated. This is added to handle an interrim state in which the CRTEX
    // producer port is reported in the "origin".
    if (callee_port != nullptr && callee_port->root().is_producer()) {
      origin["port"] = callee_port->to_json();
    }

    if (!origin.empty()) {
      result["origin"] = origin;
    }
  } else if (
      !call_kind.is_declaration() &&
      !call_kind.is_propagation_without_trace()) {
    // Never emit calls for declarations and propagations without traces.
    // Emit it for everything else.
    auto call = next_hop_json(callee, call_position, callee_port);
    if (!call.empty()) {
      result["call"] = call;
    }
  }

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
