/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <json/json.h>

namespace marianatrench {
namespace local_flow {

/**
 * Guard direction for flow constraints and normalized self-loop flows.
 * Fwd = forward reading direction (default for analysis-produced constraints).
 * Bwd = backward/contravariant reading direction.
 */
enum class Guard {
  Fwd,
  Bwd,
};

inline Guard flip_guard(Guard guard) {
  return guard == Guard::Fwd ? Guard::Bwd : Guard::Fwd;
}

/**
 * Unified label kind covering both structural and call label families.
 */
enum class LabelKind {
  // Structural kinds
  Project,
  Inject,
  ContraProject,
  ContraInject,
  // Call kinds
  Call,
  Return,
  HoCall,
  HoReturn,
  Capture,
  // Extended kinds (forward compatibility)
  Interval,
  HoCapture,
};

/**
 * Unified label type combining structural and call label information.
 * A single Label can represent any kind from LabelKind.
 *
 * For structural kinds: `value` holds the field name ("" for wildcard).
 * For call kinds: `preserves_context` and `call_site` are meaningful.
 * For Interval: `value` holds the interval encoding.
 */
struct Label {
  LabelKind kind;
  std::string value;
  bool preserves_context = false;
  std::string call_site;

  Label(
      LabelKind kind,
      std::string value = {},
      bool preserves_context = false,
      std::string call_site = {})
      : kind(kind),
        value(std::move(value)),
        preserves_context(preserves_context),
        call_site(std::move(call_site)) {}

  // Conversion from StructuralLabel
  explicit Label(const struct StructuralLabel& sl);

  bool operator==(const Label& other) const;
  bool operator!=(const Label& other) const;
  bool operator<(const Label& other) const;

  bool is_structural() const;
  bool is_call() const;

  /**
   * Returns true if this label is a call boundary (Call, HoCall, or Capture).
   * Used to identify argument labels: an Inject label immediately preceding
   * a call boundary is an argument label.
   */
  bool is_call_boundary() const;

  /**
   * Returns the inverse label:
   *   Project <-> Inject, ContraProject <-> ContraInject
   *   Call <-> Return, HoCall <-> HoReturn
   *   Capture -> Capture, HoCapture -> HoCapture
   */
  Label inverse() const;

  std::string to_string() const;
  Json::Value to_json() const;
};

using LabelPath = std::vector<Label>;

/**
 * Returns true if the label path has an odd number of contra labels
 * (ContraProject/ContraInject), indicating contravariant parity.
 * Used by reverse_guard() to compute guard direction.
 */
bool is_contra_path(const LabelPath& labels);

/**
 * Compute the guard for a self-loop from a source edge's guard and
 * the label path's contra-parity.
 */
Guard reverse_guard(Guard guard, const LabelPath& labels);

/**
 * Structural labels describe field-level data flow structure.
 * Project/Inject for covariant positions, ContraProject/ContraInject for
 * contravariant positions.
 */
enum class StructuralOp {
  Project,
  Inject,
  ContraProject,
  ContraInject,
};

struct StructuralLabel {
  StructuralOp op;
  std::string field; // "" for wildcard (*), "__keys__" for keys

  bool operator==(const StructuralLabel& other) const;
  bool operator!=(const StructuralLabel& other) const;
  bool operator<(const StructuralLabel& other) const;

  /**
   * Returns the dual (inverted) label:
   *   Project <-> ContraProject
   *   Inject <-> ContraInject
   */
  StructuralLabel dual() const;

  /**
   * Returns the inverse direction label:
   *   Project <-> Inject (field preserved)
   *   ContraProject <-> ContraInject (field preserved)
   */
  StructuralLabel inverse() const;

  std::string to_string() const;
  Json::Value to_json() const;
};

std::string structural_op_to_string(StructuralOp op);

using StructuralLabelPath = std::vector<StructuralLabel>;

} // namespace local_flow
} // namespace marianatrench
