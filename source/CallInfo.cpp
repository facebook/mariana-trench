/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/AccessPathFactory.h>
#include <mariana-trench/CallInfo.h>

namespace marianatrench {

CallInfo CallInfo::propagate(
    const Method* MT_NULLABLE callee,
    const AccessPath& callee_port,
    const Position* call_position,
    Context& context) const {
  mt_assert(!call_kind().is_propagation_without_trace());

  // CRTEX is identified by the "anchor" port, leaf-ness is identified by the
  // path() length. Once a CRTEX frame is propagated, its path is never empty.
  const auto* current_callee_port = this->callee_port();
  bool is_crtex_leaf = current_callee_port != nullptr &&
      current_callee_port->root().is_anchor() &&
      current_callee_port->path().size() == 0;
  // Callee should be present if this is a CRTEX leaf.
  mt_assert(!is_crtex_leaf || callee != nullptr);
  auto propagated_callee_port =
      is_crtex_leaf ? callee_port.canonicalize_for_method(callee) : callee_port;

  const auto* propagated_callee = callee;
  if (call_kind().is_declaration()) {
    // When propagating a declaration, set the callee to nullptr. Traces do
    // not need to point to the declaration (which may not even be a method).
    propagated_callee = nullptr;
  }

  auto propagated_call_kind = call_kind().propagate();
  return CallInfo(
      propagated_callee,
      propagated_call_kind,
      context.access_path_factory->get(propagated_callee_port),
      call_position);
}

CallInfo CallInfo::from_json(const Json::Value& value, Context& context) {
  auto call_info = JsonValidation::object(value, "call_info");

  auto call_kind = CallKind::from_trace_string(
      JsonValidation::string(call_info, "call_kind"));

  const Method* callee = nullptr;
  if (call_info.isMember("resolves_to")) {
    callee = Method::from_json(call_info["resolves_to"], context);
  }

  const Position* position = nullptr;
  if (call_info.isMember("position")) {
    position = Position::from_json(call_info["position"], context);
  }

  const AccessPath* port = nullptr;
  if (call_info.isMember("port")) {
    port = context.access_path_factory->get(
        AccessPath::from_json(call_info["port"]));
  }

  return CallInfo(callee, call_kind, port, position);
}

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
    // TODO(T176362886): Looks like Leaf port is only serving as a placeholder
    // and is semantically equivalent to nullptr. Remove it since from_json()
    // cannot deterministically re-create the exact same structure as it does
    // not know whether to use nullptr or Leaf. Should not affect anything in
    // practice, but unit tests doing EXPECT_EQ(from_json(obj.to_json()), obj)
    // cannot perform the == comparison correctly.
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
