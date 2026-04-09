/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <map>
#include <unordered_set>

#include <IRInstruction.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/local_flow/LocalFlowConstraint.h>
#include <mariana-trench/local_flow/LocalFlowNode.h>
#include <mariana-trench/local_flow/LocalFlowValueTree.h>

struct DexPosition;
class DexType;

namespace marianatrench {

// Forward declarations for optional dependencies
class Positions;
class CallGraph;

namespace local_flow {

/**
 * Register environment mapping Redex registers to value trees.
 * This is the local state maintained during the single-pass CFG walk.
 */
using RegisterEnvironment = std::map<reg_t, LocalFlowValueTree>;

/**
 * Per-instruction transfer function for local flow analysis.
 *
 * Maintains a register->value-tree map and emits raw LocalFlowConstraints.
 * This is NOT a fixpoint transfer -- it processes each instruction once
 * during a single forward pass over the CFG.
 *
 * Supports two modes for callee resolution:
 *  - Rich mode (call_graph != nullptr): uses call graph for precise dispatch
 *  - Lightweight mode (call_graph == nullptr): falls back to string-based
 *    show(method_ref)
 *
 * When positions are provided, Call/Return labels carry source file:line
 * for local_flow_explorer navigation.
 */
class LocalFlowTransfer {
 public:
  explicit LocalFlowTransfer(
      const Method* method,
      TempIdGenerator& temp_gen,
      const Positions* MT_NULLABLE positions = nullptr,
      const CallGraph* MT_NULLABLE call_graph = nullptr);

  /**
   * Process a single instruction, updating register environment
   * and emitting constraints as needed.
   *
   * @param position The DexPosition in effect at this instruction (from
   *   preceding MFLOW_POSITION entries). Used for Call/Return label
   *   annotation when positions_ is available. May be nullptr.
   */
  void analyze_instruction(
      const IRInstruction* instruction,
      RegisterEnvironment& env,
      LocalFlowConstraintSet& constraints,
      const DexPosition* MT_NULLABLE position = nullptr);

  /**
   * Get the pending invoke result (set by INVOKE_*, consumed by MOVE_RESULT).
   */
  const std::optional<LocalFlowValueTree>& pending_result() const;

  /**
   * Returns the register currently holding the `this` parameter (p0),
   * or std::nullopt for static methods.
   */
  std::optional<reg_t> this_register() const;

  /**
   * Returns the set of DexType*s encountered during analysis (receiver types,
   * new-instance types). Used for demand-driven class dispatch model
   * generation.
   */
  const std::unordered_set<const DexType*>& relevant_types() const;

 private:
  void analyze_load_param(
      const IRInstruction* instruction,
      RegisterEnvironment& env);

  void analyze_move(const IRInstruction* instruction, RegisterEnvironment& env);

  void analyze_const(
      const IRInstruction* instruction,
      RegisterEnvironment& env);

  void analyze_iget(
      const IRInstruction* instruction,
      RegisterEnvironment& env,
      LocalFlowConstraintSet& constraints);

  void analyze_iput(
      const IRInstruction* instruction,
      RegisterEnvironment& env,
      LocalFlowConstraintSet& constraints);

  void analyze_sget(const IRInstruction* instruction, RegisterEnvironment& env);

  void analyze_sput(
      const IRInstruction* instruction,
      RegisterEnvironment& env,
      LocalFlowConstraintSet& constraints);

  void analyze_aget(
      const IRInstruction* instruction,
      RegisterEnvironment& env,
      LocalFlowConstraintSet& constraints);

  void analyze_aput(
      const IRInstruction* instruction,
      RegisterEnvironment& env,
      LocalFlowConstraintSet& constraints);

  void analyze_invoke(
      const IRInstruction* instruction,
      RegisterEnvironment& env,
      LocalFlowConstraintSet& constraints,
      const DexPosition* MT_NULLABLE position = nullptr);

  void analyze_move_result(
      const IRInstruction* instruction,
      RegisterEnvironment& env);

  void analyze_return(
      const IRInstruction* instruction,
      RegisterEnvironment& env,
      LocalFlowConstraintSet& constraints);

  void analyze_new_instance(
      const IRInstruction* instruction,
      RegisterEnvironment& env);

  void analyze_check_cast(
      const IRInstruction* instruction,
      RegisterEnvironment& env);

  void analyze_filled_new_array(
      const IRInstruction* instruction,
      RegisterEnvironment& env,
      LocalFlowConstraintSet& constraints);

  void analyze_throw(
      const IRInstruction* instruction,
      RegisterEnvironment& env,
      LocalFlowConstraintSet& constraints);

  /**
   * Look up the value tree for a register, creating a fresh temp if not found.
   */
  const LocalFlowValueTree& get_or_create(RegisterEnvironment& env, reg_t reg);

  /**
   * Get the callee node for an invoke instruction.
   */
  LocalFlowNode get_callee_node(const IRInstruction* instruction) const;

  /**
   * Check if a register holds the `this` parameter (p0 in instance methods).
   */
  bool is_this_register(reg_t reg) const;

  /**
   * Format call site string with per-invocation uniqueness.
   * Uses (line,index) format where index is a per-method counter.
   */
  std::string format_call_site(const DexPosition* MT_NULLABLE position);

  /**
   * Returns true if this invoke should be skipped (intrinsic checks,
   * trivial Object.<init>).
   */
  bool should_skip_invoke(const IRInstruction* instruction) const;

  const Method* method_;
  TempIdGenerator& temp_gen_;
  const Positions* MT_NULLABLE positions_;
  const CallGraph* MT_NULLABLE call_graph_;
  uint32_t next_param_index_;
  bool is_static_;
  std::optional<LocalFlowValueTree> pending_result_;

  // Track which register holds each parameter for `this` detection
  std::map<reg_t, uint32_t> param_registers_;

  // Types encountered during analysis for demand-driven class models
  std::unordered_set<const DexType*> relevant_types_;

  // Per-method invoke counter for unique call site labels
  uint32_t invoke_counter_ = 0;
};

} // namespace local_flow
} // namespace marianatrench
