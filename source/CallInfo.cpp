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

  auto call_info = next_hop_json();
  result["call_info"] = call_info;

  // TODO(T163918472): Remove differentiation between origin and non-origins.
  // This structure can be emitted as a single JSON "call_info" key as done
  // above. Cannot remove this yet as the parser has not been updated.
  if (call_kind.is_origin()) {
    auto origin = origin_json(call_position);

    // TODO(T163918472): Parser currently relies on this to populate callee
    // information.
    if (callee != nullptr) {
      origin["method"] = callee->to_json();
    }

    // TODO(T163918472): Parser relies on this for CRTEX ports. Otherwise,
    // it expects "port" to be absent in origin frames.
    if (callee_port != nullptr && callee_port->root().is_producer()) {
      origin["port"] = callee_port->to_json();
    }

    if (!origin.empty()) {
      result["origin"] = origin;
    }
  } else if (
      !call_kind.is_declaration() &&
      !call_kind.is_propagation_without_trace()) {
    // TODO(T163918472): Remove in favor of "call_info" after parser update.
    result["call"] = call_info;
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

Json::Value CallInfo::next_hop_json() const {
  // TODO(T163918472): For call_kind == origin, ensure the following
  // invariants: callee == nullptr && callee_port.is_leaf()
  auto call = Json::Value(Json::objectValue);
  call["call_kind"] = call_kind().to_trace_string();
  const auto* callee = this->callee();
  if (callee != nullptr) {
    call["resolves_to"] = callee->to_json();
  }
  if (call_position_ != nullptr) {
    call["position"] = call_position_->to_json();
  }
  if (callee_port_ != nullptr && !callee_port_->root().is_leaf()) {
    call["port"] = callee_port_->to_json();
  }
  return call;
}

} // namespace marianatrench
