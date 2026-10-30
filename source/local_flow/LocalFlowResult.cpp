/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowResult.h>

#include <fmt/format.h>

#include <mariana-trench/Assert.h>

namespace marianatrench {
namespace local_flow {

namespace {

// Label that reads the value of `node` at the start of a flow. A statically
// resolved callee is entered without a member label, and a constant has no
// member to select.
std::optional<Label> source_label(const LocalFlowNode& node) {
  switch (node.kind()) {
    case NodeKind::Global:
    case NodeKind::Constant:
      return std::nullopt;
    case NodeKind::Meth:
    case NodeKind::ExactMeth:
    case NodeKind::StaticField:
      return Label{
          LabelKind::Project, LocalFlowNode::extract_vtable_name(node)};
    case NodeKind::Param:
    case NodeKind::Result:
    case NodeKind::This:
    case NodeKind::ThisOut:
    case NodeKind::Ctrl:
    case NodeKind::CVar:
    case NodeKind::OVar:
    case NodeKind::Temp:
      return Label{LabelKind::ContraProject, node.to_string()};
  }
  mt_unreachable();
}

// Label that writes the value into `node` at the end of a flow.
std::optional<Label> target_label(const LocalFlowNode& node) {
  switch (node.kind()) {
    case NodeKind::Global:
      return std::nullopt;
    case NodeKind::Meth:
    case NodeKind::ExactMeth:
    case NodeKind::StaticField:
      return Label{LabelKind::Inject, LocalFlowNode::extract_vtable_name(node)};
    case NodeKind::Param:
    case NodeKind::Result:
    case NodeKind::This:
    case NodeKind::ThisOut:
    case NodeKind::Ctrl:
    case NodeKind::CVar:
    case NodeKind::OVar:
    case NodeKind::Constant:
    case NodeKind::Temp:
      return Label{LabelKind::Inject, node.to_string()};
  }
  mt_unreachable();
}

// Node that a flow names as its `left` or `right` end. Roots belong to the
// method itself. A dispatched callee is reached through `O{class}`, which
// enters the override hierarchy, and a callee called without dispatch through
// `C{class}`, which holds only the class's own methods; the member label
// selects the method. Every other node is named by its own spelling.
std::string edge_endpoint(
    const LocalFlowNode& node,
    const std::string& callable_node) {
  switch (node.kind()) {
    case NodeKind::Param:
    case NodeKind::Result:
    case NodeKind::This:
    case NodeKind::ThisOut:
    case NodeKind::Ctrl:
      return callable_node;
    case NodeKind::Meth:
      return fmt::format("O{{{}}}", LocalFlowNode::extract_class_name(node));
    case NodeKind::ExactMeth:
      return fmt::format("C{{{}}}", LocalFlowNode::extract_class_name(node));
    case NodeKind::Global:
    case NodeKind::StaticField:
    case NodeKind::CVar:
    case NodeKind::OVar:
    case NodeKind::Constant:
    case NodeKind::Temp:
      return node.to_string();
  }
  mt_unreachable();
}

} // namespace

Json::Value LocalFlowMethodResult::to_json() const {
  // Instance methods use M{} (convention for callers to reference them via
  // virtual/interface dispatch). Static methods use G{} (statically resolved,
  // no dispatch table).
  auto callable_node = is_static ? fmt::format("G{{{}}}", callable)
                                 : fmt::format("M{{{}}}", callable);

  auto flows_json = Json::Value(Json::arrayValue);
  for (const auto& constraint : constraints.constraints()) {
    // Label path: <source label> + constraint labels + <target label>.
    LabelPath self_loop_labels;
    self_loop_labels.reserve(constraint.labels.size() + 2);
    if (auto label = source_label(constraint.source)) {
      self_loop_labels.push_back(std::move(*label));
    }
    for (const auto& label : constraint.labels) {
      self_loop_labels.push_back(label);
    }
    if (auto label = target_label(constraint.target)) {
      self_loop_labels.push_back(std::move(*label));
    }

    Guard guard = projection_guard(constraint.guard, self_loop_labels);

    auto flow = Json::Value(Json::objectValue);
    // Only emit guard for Bwd
    if (guard == Guard::Bwd) {
      flow["guard"] = "Bwd";
    }
    flow["left"] = edge_endpoint(constraint.source, callable_node);
    flow["right"] = edge_endpoint(constraint.target, callable_node);

    auto labels_json = Json::Value(Json::arrayValue);
    for (const auto& label : self_loop_labels) {
      labels_json.append(label.to_string());
    }
    flow["labels"] = labels_json;

    flows_json.append(std::move(flow));
  }

  auto localflow = Json::Value(Json::objectValue);
  localflow["node"] = callable_node;
  localflow["flows"] = flows_json;

  auto result = Json::Value(Json::objectValue);
  result["code"] = 20000;
  result["callable"] = callable;
  result["path"] = path;
  result["callable_line"] = callable_line;
  result["localflow"] = localflow;

  if (method_metadata.has_value()) {
    auto meta_json = Json::Value(Json::objectValue);

    auto params_json = Json::Value(Json::arrayValue);
    for (const auto& pt : method_metadata->param_types) {
      params_json.append(pt);
    }
    meta_json["param_types"] = params_json;
    meta_json["return_type"] = method_metadata->return_type;
    meta_json["is_external"] = method_metadata->is_external;

    auto annos_json = Json::Value(Json::arrayValue);
    for (const auto& a : method_metadata->annotations) {
      annos_json.append(a);
    }
    meta_json["annotations"] = annos_json;

    result["method_metadata"] = meta_json;
  }

  return result;
}

Json::Value LocalFlowClassResult::to_json() const {
  auto flows_json = Json::Value(Json::arrayValue);
  for (const auto& edge : dispatch_edges) {
    auto flow = Json::Value(Json::objectValue);
    flow["left"] = edge.source.to_json();
    flow["right"] = edge.target.to_json();

    if (!edge.labels.empty()) {
      auto labels_json = Json::Value(Json::arrayValue);
      for (const auto& label : edge.labels) {
        labels_json.append(label.to_json());
      }
      flow["labels"] = labels_json;
    }
    // No "guard" for class flows
    flows_json.append(std::move(flow));
  }

  auto localflow = Json::Value(Json::objectValue);
  localflow["node"] = fmt::format("C{{{}}}", class_name);
  localflow["interval"] =
      interval.empty() ? Json::nullValue : Json::Value(interval);
  localflow["flows"] = flows_json;

  auto result = Json::Value(Json::objectValue);
  result["code"] = 20001;
  result["callable"] = fmt::format("Class{{{}}}", class_name);
  result["path"] = path;
  result["callable_line"] = callable_line;
  result["localflow"] = localflow;
  return result;
}

std::ostream& operator<<(
    std::ostream& out,
    const LocalFlowMethodResult& result) {
  out << result.callable;
  if (!result.path.empty()) {
    out << "  [" << result.path << ":" << result.callable_line << "]";
  }
  out << "\n";
  for (const auto& c : result.constraints.constraints()) {
    out << "  " << c.source.to_string();
    if (c.labels.empty()) {
      out << " -> ";
    } else {
      out << " -[";
      for (std::size_t i = 0; i < c.labels.size(); ++i) {
        if (i > 0) {
          out << ", ";
        }
        out << c.labels[i].to_string();
      }
      out << "]-> ";
    }
    out << c.target.to_string() << "\n";
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const DispatchEdge& edge) {
  out << edge.source.to_string();
  if (edge.labels.empty()) {
    out << " -> ";
  } else {
    out << " -[";
    for (std::size_t i = 0; i < edge.labels.size(); ++i) {
      if (i > 0) {
        out << ", ";
      }
      out << edge.labels[i].to_string();
    }
    out << "]-> ";
  }
  out << edge.target.to_string();
  return out;
}

std::ostream& operator<<(
    std::ostream& out,
    const LocalFlowClassResult& result) {
  out << "Class{" << result.class_name << "}";
  if (!result.interval.empty()) {
    out << " [interval=" << result.interval << "]";
  }
  out << "\n";
  for (const auto& edge : result.dispatch_edges) {
    out << "  " << edge << "\n";
  }
  return out;
}

} // namespace local_flow
} // namespace marianatrench
