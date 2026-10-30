/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

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
  // Call kinds. Consumers derive the higher-order kinds from these.
  Call,
  Return,
  // Class interval of an override edge, read as a call label.
  Interval,
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

  explicit Label(
      LabelKind kind,
      std::string value = {},
      bool preserves_context = false,
      std::string call_site = {})
      : kind(kind),
        value(std::move(value)),
        preserves_context(preserves_context),
        call_site(std::move(call_site)) {}

  bool operator==(const Label& other) const;
  bool operator!=(const Label& other) const;
  bool operator<(const Label& other) const;

  bool is_structural() const;
  bool is_call() const;

  /**
   * Returns the inverse label:
   *   Project <-> Inject, ContraProject <-> ContraInject
   *   Call <-> Return, Interval -> Interval
   */
  Label inverse() const;

  std::string to_string() const;
  Json::Value to_json() const;
};

using LabelPath = std::vector<Label>;

/**
 * Guard of an emitted edge: `guard`, flipped when the leading projections of
 * `labels` (call labels skipped) contain an odd number of ContraProject, i.e.
 * when the edge starts by consuming an argument. Consumers pick the edges to
 * follow by this guard, so an edge read from a parameter (the receiver `p0`
 * included) or `ctrl` is Bwd and an edge read from a callee or a constant is
 * Fwd.
 */
Guard projection_guard(Guard guard, const LabelPath& labels);

} // namespace local_flow
} // namespace marianatrench
