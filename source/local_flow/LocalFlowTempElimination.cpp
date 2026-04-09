/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowTempElimination.h>

#include <deque>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace marianatrench {
namespace local_flow {

namespace {

// Parenthesis-matching label cancellation. Returns std::nullopt if the
// composition produces an incompatible path.
//
// Convention: `top` = stack.back() (accumulated left edge labels, reversed),
//             `label` = incoming label (from right edge).
//
// Rules:
// - Inject(x)/Project(x): cancel if match or wildcard; KILL if mismatch
// - ContraInject(x)/ContraProject(x): cancel if match; KILL if mismatch
// - Cross-contra: top=ContraInject+incoming=Project -> KILL;
//                 top=Project+incoming=ContraInject -> stack (valid cross-call)
// - Call(A)/Return(A): cancel if match; KILL if mismatch
// - HoCall/Call: cancel if match; stack if mismatch (context-insensitive
// approx)
// - Capture: commutes past Call, absorbs Return
std::optional<LabelPath> cons_label(const Label& label, LabelPath stack) {
  if (!stack.empty()) {
    const auto& top = stack.back();

    // --- Structural cancellation ---

    // Inject(x) + Project(y): cancel if match/wildcard, KILL if mismatch
    if (top.kind == LabelKind::Inject && label.kind == LabelKind::Project) {
      if (top.value == label.value || top.value == "*" || label.value == "*") {
        stack.pop_back();
        return stack;
      }
      return std::nullopt; // mismatched fields: kill
    }

    // ContraInject(x) + ContraProject(y): cancel if match, KILL if mismatch
    if (top.kind == LabelKind::ContraInject &&
        label.kind == LabelKind::ContraProject) {
      if (top.value == label.value) {
        stack.pop_back();
        return stack;
      }
      return std::nullopt; // mismatched contra fields: kill
    }

    // Cross-contra: top=ContraInject + incoming=Project -> KILL
    // (entered parameter position, trying to exit via result — invalid)
    if (top.kind == LabelKind::ContraInject &&
        label.kind == LabelKind::Project) {
      return std::nullopt;
    }
    // Cross-contra: top=Inject + incoming=ContraProject -> KILL
    if (top.kind == LabelKind::Inject &&
        label.kind == LabelKind::ContraProject) {
      return std::nullopt;
    }
    // Reverse direction (top=Project, incoming=ContraInject) is valid
    // cross-call composition — falls through to default push.

    // --- Call/Return cancellation (direction: top=Call, incoming=Return) ---

    // Call(A) + Return(B): cancel if match, KILL if mismatch
    if (top.kind == LabelKind::Call && label.kind == LabelKind::Return) {
      if (top.call_site == label.call_site) {
        stack.pop_back();
        return stack;
      }
      return std::nullopt; // mismatched call sites: impossible path
    }

    // HoReturn(A) + HoCall(B): cancel if match, KILL if mismatch
    if (top.kind == LabelKind::HoReturn && label.kind == LabelKind::HoCall) {
      if (top.call_site == label.call_site) {
        stack.pop_back();
        return stack;
      }
      return std::nullopt;
    }

    // HoReturn(A) + Return(B): cancel if match, KILL if mismatch
    if (top.kind == LabelKind::HoReturn && label.kind == LabelKind::Return) {
      if (top.call_site == label.call_site) {
        stack.pop_back();
        return stack;
      }
      return std::nullopt;
    }

    // HoCall(A) + Call(B): cancel if match; stack if mismatch
    // (context-insensitive approximation for higher-order calls)
    if (top.kind == LabelKind::HoCall && label.kind == LabelKind::Call) {
      if (top.call_site == label.call_site) {
        stack.pop_back();
        return stack;
      }
      // mismatch: just stack (don't kill — HoCall nesting is allowed)
    }

    // Capture commutes past Call: Capture :: rest, Call -> reorder
    if (top.kind == LabelKind::Capture && label.is_call_boundary()) {
      stack.pop_back(); // remove Capture
      stack.push_back(label); // push Call/HoCall
      stack.push_back(Label{LabelKind::Capture}); // push Capture back on top
      return stack;
    }

    // Capture absorbs Return
    if (top.kind == LabelKind::Capture && label.kind == LabelKind::Return) {
      // Return is absorbed; Capture remains on stack
      return stack;
    }

    // Capture under Call: Capture :: (Call l1 :: ...), Return l2
    // If Call/Return match, cancel the Call but keep Capture
    if (top.kind == LabelKind::Call && label.kind == LabelKind::Return &&
        stack.size() >= 2) {
      const auto& under_top = stack[stack.size() - 2];
      if (under_top.kind == LabelKind::Capture &&
          top.call_site == label.call_site) {
        stack.pop_back(); // remove Call (matched by Return)
        // Capture remains
        return stack;
      }
    }

    // HoReturn + HoCapture -> cancel
    if (top.kind == LabelKind::HoReturn && label.kind == LabelKind::HoCapture) {
      stack.pop_back();
      return stack;
    }

    // HoCapture + Capture -> cancel
    if (top.kind == LabelKind::HoCapture && label.kind == LabelKind::Capture) {
      stack.pop_back();
      return stack;
    }
  }

  // Default: push label onto stack
  stack.push_back(label);
  return stack;
}

// Fold cons_label over a label path. Returns nullopt if any step fails.
std::optional<LabelPath> cons_labels(LabelPath stack, const LabelPath& labels) {
  for (const auto& label : labels) {
    auto result = cons_label(label, std::move(stack));
    if (!result.has_value()) {
      return std::nullopt;
    }
    stack = std::move(*result);
  }
  return stack;
}

// Internal edge during closure.
// Structural and call labels are stored separately (in reverse/stack order)
// for independent composition with cancellation.
struct Edge {
  Guard guard;
  LocalFlowNode source;
  LocalFlowNode target;
  LabelPath struct_stack; // structural labels accumulated as a stack
  LabelPath call_stack; // call labels accumulated as a stack
};

// Partition a unified label path into structural and call labels
void partition_labels(
    const LabelPath& labels,
    LabelPath& out_struct,
    LabelPath& out_call) {
  for (const auto& label : labels) {
    if (label.is_structural()) {
      out_struct.push_back(label);
    } else {
      out_call.push_back(label);
    }
  }
}

// Re-merge: call labels first, then structural labels
LabelPath merge_labels(
    const LabelPath& struct_stack,
    const LabelPath& call_stack) {
  LabelPath result;
  result.reserve(call_stack.size() + struct_stack.size());
  result.insert(result.end(), call_stack.begin(), call_stack.end());
  result.insert(result.end(), struct_stack.begin(), struct_stack.end());
  return result;
}

// Create an Edge from a constraint (partitioning labels)
Edge make_edge(const LocalFlowConstraint& c) {
  LabelPath struct_labels, call_labels;
  partition_labels(c.labels, struct_labels, call_labels);
  return Edge{
      c.guard,
      c.source,
      c.target,
      std::move(struct_labels),
      std::move(call_labels)};
}

// Convert Edge to constraint with re-merged labels
LocalFlowConstraint make_constraint(const Edge& edge) {
  auto labels = merge_labels(edge.struct_stack, edge.call_stack);
  return LocalFlowConstraint{
      edge.source, std::move(labels), edge.target, edge.guard};
}

// Compose two edges: ab + bc -> ac, with label cancellation.
// Returns nullopt if cancellation produces an incompatible path.
std::optional<Edge> compose(const Edge& ab, const Edge& bc) {
  auto composed_struct = cons_labels(ab.struct_stack, bc.struct_stack);
  if (!composed_struct.has_value()) {
    return std::nullopt;
  }

  auto composed_call = cons_labels(ab.call_stack, bc.call_stack);
  if (!composed_call.has_value()) {
    return std::nullopt;
  }

  return Edge{
      ab.guard, // inherit guard from source edge
      ab.source,
      bc.target,
      std::move(*composed_struct),
      std::move(*composed_call)};
}

} // namespace

// Maximum number of edges composed during closure. Prevents OOM from
// exponential blowup in methods with dense temp graphs (builder patterns,
// fluent APIs, deeply nested lambdas). When exceeded, the method's constraints
// are cleared (no local flow summary emitted for that method).
constexpr std::size_t k_max_composed_edges = 100000;

bool LocalFlowTempElimination::eliminate(
    LocalFlowConstraintSet& constraints,
    std::size_t max_structural_depth) {
  // Build adjacency maps using internal Edge representation
  std::map<std::string, std::vector<Edge>> outgoing;
  std::map<std::string, std::vector<Edge>> incoming;

  for (const auto& c : constraints.constraints()) {
    auto edge = make_edge(c);
    outgoing[c.source.to_string()].push_back(edge);
    incoming[c.target.to_string()].push_back(edge);
  }

  // Worklist-based closure
  std::deque<Edge> worklist;

  // Seed worklist with all edges from non-temp sources
  for (const auto& c : constraints.constraints()) {
    if (!c.source.is_temp()) {
      worklist.push_back(make_edge(c));
    }
  }

  // Track discovered final edges to avoid duplicates
  std::set<LocalFlowConstraint> final_edges;

  // Dedup set for ALL composed edges (intermediate + final).
  // Each unique (source, target, labels, guard) edge is processed exactly once,
  // preventing exponential blowup from cycles created by loop-head temps.
  std::set<LocalFlowConstraint> seen_edges;

  // Forward pass: extend edges from non-temp sources through temps
  while (!worklist.empty()) {
    auto edge = std::move(worklist.front());
    worklist.pop_front();

    if (!edge.target.is_temp()) {
      final_edges.insert(make_constraint(edge));
      continue;
    }

    auto it = outgoing.find(edge.target.to_string());
    if (it == outgoing.end()) {
      continue;
    }

    for (const auto& next_edge : it->second) {
      auto composed = compose(edge, next_edge);
      if (!composed.has_value()) {
        continue; // incompatible path (cancelled out)
      }
      if (composed->struct_stack.size() <= max_structural_depth) {
        auto constraint = make_constraint(*composed);
        if (seen_edges.insert(constraint).second) {
          worklist.push_back(std::move(*composed));
          // Safety valve (should rarely trigger with dedup)
          if (seen_edges.size() >= k_max_composed_edges) {
            constraints = LocalFlowConstraintSet();
            return false;
          }
        }
      }
    }
  }

  // Backward pass: trace backward from non-temp targets through temp sources
  std::deque<Edge> backward_worklist;
  for (const auto& c : constraints.constraints()) {
    if (!c.target.is_temp()) {
      backward_worklist.push_back(make_edge(c));
    }
  }

  while (!backward_worklist.empty()) {
    auto edge = std::move(backward_worklist.front());
    backward_worklist.pop_front();

    if (!edge.source.is_temp()) {
      continue; // already handled in forward pass
    }

    auto it = incoming.find(edge.source.to_string());
    if (it == incoming.end()) {
      continue;
    }

    for (const auto& prev_edge : it->second) {
      auto composed = compose(prev_edge, edge);
      if (!composed.has_value()) {
        continue;
      }
      if (composed->struct_stack.size() <= max_structural_depth) {
        auto constraint = make_constraint(*composed);
        if (seen_edges.insert(constraint).second) {
          if (!composed->source.is_temp()) {
            final_edges.insert(std::move(constraint));
          } else {
            backward_worklist.push_back(std::move(*composed));
          }
          // Safety valve
          if (seen_edges.size() >= k_max_composed_edges) {
            constraints = LocalFlowConstraintSet();
            return false;
          }
        }
      }
    }
  }

  // Replace constraint set with final edges
  LocalFlowConstraintSet result;
  for (auto& constraint : final_edges) {
    result.add(
        LocalFlowConstraint{
            constraint.source,
            constraint.labels,
            constraint.target,
            constraint.guard});
  }
  result.deduplicate();
  constraints = std::move(result);
  return true;
}

} // namespace local_flow
} // namespace marianatrench
