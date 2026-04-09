/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ostream>
#include <string>
#include <unordered_set>

#include <json/json.h>

#include <mariana-trench/local_flow/LocalFlowConstraint.h>

class DexType;

namespace marianatrench {
namespace local_flow {

/**
 * Per-callable local flow result: final constraints + metadata.
 */
struct LocalFlowMethodResult {
  std::string callable; // Method signature
  LocalFlowConstraintSet constraints;

  // Types encountered during analysis (not serialized — used for
  // demand-driven class dispatch model generation).
  std::unordered_set<const DexType*> relevant_types;

  std::string path; // Source file path (relative)
  int callable_line = 0; // Line number of the method definition
  bool is_static = false; // Static methods use G{}, instance methods use M{}

  LocalFlowMethodResult(
      std::string callable,
      LocalFlowConstraintSet constraints,
      std::unordered_set<const DexType*> relevant_types = {},
      std::string path = {},
      int callable_line = 0,
      bool is_static = false)
      : callable(std::move(callable)),
        constraints(std::move(constraints)),
        relevant_types(std::move(relevant_types)),
        path(std::move(path)),
        callable_line(callable_line),
        is_static(is_static) {}

  /**
   * Serialize as normalized self-loop format (code 20000).
   * This is the format expected by local_flow_explorer.
   */
  Json::Value to_json() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const LocalFlowMethodResult& result);
};

/**
 * Dispatch edge in a class result.
 */
struct DispatchEdge {
  LocalFlowNode source;
  StructuralLabelPath labels;
  LocalFlowNode target;

  friend std::ostream& operator<<(std::ostream& out, const DispatchEdge& edge);
};

/**
 * Per-class local flow result: dispatch/hierarchy edges.
 */
struct LocalFlowClassResult {
  std::string class_name;
  std::vector<DispatchEdge> dispatch_edges;
  std::string path; // Source file path
  int callable_line = 0; // Class definition line
  std::string interval; // Class interval, e.g. "[85,85]"

  /**
   * Serialize as class result (code 20001).
   */
  Json::Value to_json() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const LocalFlowClassResult& result);
};

} // namespace local_flow
} // namespace marianatrench
