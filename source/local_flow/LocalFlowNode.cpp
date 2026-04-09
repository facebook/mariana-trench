/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowNode.h>

#include <fmt/format.h>

namespace marianatrench {
namespace local_flow {

LocalFlowNode::LocalFlowNode(NodeKind kind, Payload payload)
    : kind_(kind), payload_(std::move(payload)) {}

LocalFlowNode LocalFlowNode::make_param(uint32_t index) {
  return LocalFlowNode(NodeKind::Param, ParamPayload{index});
}

LocalFlowNode LocalFlowNode::make_result() {
  return LocalFlowNode(NodeKind::Result, NoPayload{});
}

LocalFlowNode LocalFlowNode::make_this() {
  return LocalFlowNode(NodeKind::This, NoPayload{});
}

LocalFlowNode LocalFlowNode::make_this_out() {
  return LocalFlowNode(NodeKind::ThisOut, NoPayload{});
}

LocalFlowNode LocalFlowNode::make_ctrl() {
  return LocalFlowNode(NodeKind::Ctrl, NoPayload{});
}

LocalFlowNode LocalFlowNode::make_global(std::string name) {
  return LocalFlowNode(NodeKind::Global, NamePayload{std::move(name)});
}

LocalFlowNode LocalFlowNode::make_meth(std::string name) {
  return LocalFlowNode(NodeKind::Meth, NamePayload{std::move(name)});
}

LocalFlowNode LocalFlowNode::make_cvar(std::string name) {
  return LocalFlowNode(NodeKind::CVar, NamePayload{std::move(name)});
}

LocalFlowNode LocalFlowNode::make_ovar(std::string name) {
  return LocalFlowNode(NodeKind::OVar, NamePayload{std::move(name)});
}

LocalFlowNode LocalFlowNode::make_self_rec() {
  return LocalFlowNode(NodeKind::SelfRec, NoPayload{});
}

LocalFlowNode LocalFlowNode::make_constant(std::string value) {
  return LocalFlowNode(NodeKind::Constant, NamePayload{std::move(value)});
}

LocalFlowNode LocalFlowNode::make_temp(uint64_t id) {
  return LocalFlowNode(NodeKind::Temp, TempPayload{id});
}

NodeKind LocalFlowNode::kind() const {
  return kind_;
}

bool LocalFlowNode::is_temp() const {
  return kind_ == NodeKind::Temp;
}

bool LocalFlowNode::is_root() const {
  switch (kind_) {
    case NodeKind::Param:
    case NodeKind::Result:
    case NodeKind::This:
    case NodeKind::ThisOut:
    case NodeKind::Ctrl:
      return true;
    default:
      return false;
  }
}

bool LocalFlowNode::is_global() const {
  switch (kind_) {
    case NodeKind::Global:
    case NodeKind::Meth:
    case NodeKind::CVar:
    case NodeKind::OVar:
    case NodeKind::SelfRec:
    case NodeKind::Constant:
      return true;
    default:
      return false;
  }
}

bool LocalFlowNode::is_callee_node() const {
  return kind_ == NodeKind::Global || kind_ == NodeKind::Meth;
}

bool LocalFlowNode::operator==(const LocalFlowNode& other) const {
  if (kind_ != other.kind_) {
    return false;
  }
  if (auto* p = std::get_if<ParamPayload>(&payload_)) {
    return p->index == std::get<ParamPayload>(other.payload_).index;
  }
  if (auto* n = std::get_if<NamePayload>(&payload_)) {
    return n->name == std::get<NamePayload>(other.payload_).name;
  }
  if (auto* t = std::get_if<TempPayload>(&payload_)) {
    return t->id == std::get<TempPayload>(other.payload_).id;
  }
  // NoPayload - kinds already match
  return true;
}

bool LocalFlowNode::operator!=(const LocalFlowNode& other) const {
  return !(*this == other);
}

bool LocalFlowNode::operator<(const LocalFlowNode& other) const {
  if (kind_ != other.kind_) {
    return kind_ < other.kind_;
  }
  if (auto* p = std::get_if<ParamPayload>(&payload_)) {
    return p->index < std::get<ParamPayload>(other.payload_).index;
  }
  if (auto* n = std::get_if<NamePayload>(&payload_)) {
    return n->name < std::get<NamePayload>(other.payload_).name;
  }
  if (auto* t = std::get_if<TempPayload>(&payload_)) {
    return t->id < std::get<TempPayload>(other.payload_).id;
  }
  // NoPayload - equal
  return false;
}

std::string LocalFlowNode::jvm_to_lfe_separator(const std::string& jvm_sig) {
  // Find ";." which is the class-type-end + method/field separator in JVM.
  // "Lcom/facebook/Foo;.bar:(I)V" → "Lcom/facebook/Foo;||bar:(I)V"
  // Class-only names like "Lcom/facebook/Foo;" have no ";." so pass through.
  auto pos = jvm_sig.find(";.");
  if (pos == std::string::npos) {
    return jvm_sig;
  }
  std::string result = jvm_sig;
  result.replace(pos + 1, 1, "||");
  return result;
}

std::string LocalFlowNode::extract_vtable_name(const LocalFlowNode& node) {
  if (node.kind_ != NodeKind::Global && node.kind_ != NodeKind::Meth) {
    return node.to_string();
  }
  const auto& name = std::get<NamePayload>(node.payload_).name;
  auto sep_pos = name.find("||");
  if (sep_pos == std::string::npos) {
    return name;
  }
  // Return everything after "||" — keep full signature for Java overload
  // disambiguation. E.g., "Lcom/Foo;||bar:(I)V" → "bar:(I)V"
  return name.substr(sep_pos + 2);
}

std::string LocalFlowNode::extract_class_name(const LocalFlowNode& node) {
  if (node.kind_ != NodeKind::Meth && node.kind_ != NodeKind::Global) {
    return node.to_string();
  }
  const auto& name = std::get<NamePayload>(node.payload_).name;
  auto sep_pos = name.find("||");
  if (sep_pos == std::string::npos) {
    return name;
  }
  // Return everything before "||".
  // E.g., "Lcom/Foo;||bar:(I)V" → "Lcom/Foo;"
  return name.substr(0, sep_pos);
}

std::string LocalFlowNode::to_string() const {
  switch (kind_) {
    case NodeKind::Param: {
      auto index = std::get<ParamPayload>(payload_).index;
      return fmt::format("p{}", index);
    }
    case NodeKind::Result:
      return "result";
    case NodeKind::This:
      return "this";
    case NodeKind::ThisOut:
      return "this_out";
    case NodeKind::Ctrl:
      return "ctrl";
    case NodeKind::Global: {
      const auto& name = std::get<NamePayload>(payload_).name;
      return fmt::format("G{{{}}}", name);
    }
    case NodeKind::Meth: {
      const auto& name = std::get<NamePayload>(payload_).name;
      return fmt::format("M{{{}}}", name);
    }
    case NodeKind::CVar: {
      const auto& name = std::get<NamePayload>(payload_).name;
      return fmt::format("C{{{}}}", name);
    }
    case NodeKind::OVar: {
      const auto& name = std::get<NamePayload>(payload_).name;
      return fmt::format("O{{{}}}", name);
    }
    case NodeKind::SelfRec:
      return "SelfRec";
    case NodeKind::Constant: {
      const auto& value = std::get<NamePayload>(payload_).name;
      return fmt::format("C<{}>", value);
    }
    case NodeKind::Temp: {
      auto id = std::get<TempPayload>(payload_).id;
      return fmt::format("t{}", id);
    }
  }
  return "Unknown";
}

Json::Value LocalFlowNode::to_json() const {
  return Json::Value(to_string());
}

TempIdGenerator::TempIdGenerator() : next_id_(0) {}

uint64_t TempIdGenerator::next() {
  return next_id_.fetch_add(1, std::memory_order_relaxed);
}

} // namespace local_flow
} // namespace marianatrench
