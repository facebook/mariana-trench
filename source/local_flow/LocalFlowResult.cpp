/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowResult.h>

#include <fmt/format.h>

namespace marianatrench {
namespace local_flow {

Json::Value LocalFlowMethodResult::to_json() const {
  // Instance methods use M{} (convention for callers to reference them via
  // virtual/interface dispatch). Static methods use G{} (statically resolved,
  // no dispatch table).
  auto callable_node = is_static ? fmt::format("G{{{}}}", callable)
                                 : fmt::format("M{{{}}}", callable);

  auto flows_json = Json::Value(Json::arrayValue);
  for (const auto& constraint : constraints.constraints()) {
    // Build self-loop label path:
    // <source_label> + constraint.labels + Inject:<target_vtable>
    // Use vtable names (everything after ||) instead of full node strings
    // so LFE can match them against class dispatch edges.
    //
    // Source label kind:
    //   - ContraProject for parameter/this/ctrl sources (→ ArgProject in LFE)
    //   - Project for callee sources (Global/Meth) (→ Project in LFE)
    //   Callee output refs use Project so that Inject(callee) from Call edges
    //   can cancel with Project(callee).
    //
    // Argument Inject → ContraInject conversion:
    //   Raw constraints use Inject for arguments (enabling cross-call edge
    //   composition in temp elimination). Convert to ContraInject here so
    //   LFE sees ArgInject for port-based argument matching.
    //   An argument Inject is one immediately preceding a Call/HoCall/Capture.
    LabelPath self_loop_labels;
    self_loop_labels.reserve(constraint.labels.size() + 2);

    auto source_kind = constraint.source.is_callee_node()
        ? LabelKind::Project
        : LabelKind::ContraProject;
    self_loop_labels.emplace_back(
        source_kind, LocalFlowNode::extract_vtable_name(constraint.source));

    for (const auto& label : constraint.labels) {
      self_loop_labels.push_back(label);
    }

    self_loop_labels.emplace_back(
        LabelKind::Inject,
        LocalFlowNode::extract_vtable_name(constraint.target));

    // Compute guard via contra-parity
    Guard guard = reverse_guard(constraint.guard, self_loop_labels);

    // Serialize the flow.
    // Use actual source/target nodes for left/right (not always callable_node).
    // Root nodes (Param, Result, This, ThisOut, Ctrl) belong to the current
    // method, so they map to callable_node. Callee/constant nodes map to their
    // own to_json() representation. This creates non-self-loop edges that
    // enable process_lfgs compute_deps and LFE add_reverse_deps to discover
    // caller-callee relationships.
    auto node_to_edge_endpoint = [&](const LocalFlowNode& node) -> std::string {
      if (node.is_root()) {
        return callable_node;
      }
      // Route Meth callee references through C{} dispatch table. The method is
      // selected via the Inject:vtable_name label already in the self-loop
      // path. The dispatch table's Project:vtable_name edge cancels the Inject,
      // enabling multi-hop traversal.
      if (node.kind() == NodeKind::Meth) {
        auto class_name = LocalFlowNode::extract_class_name(node);
        return fmt::format("C{{{}}}", class_name);
      }
      return node.to_string();
    };

    auto flow = Json::Value(Json::objectValue);
    // Only emit guard for Bwd
    if (guard == Guard::Bwd) {
      flow["guard"] = "Bwd";
    }
    flow["left"] = node_to_edge_endpoint(constraint.source);
    flow["right"] = node_to_edge_endpoint(constraint.target);

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
