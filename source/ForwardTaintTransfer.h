/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <InstructionAnalyzer.h>

#include <mariana-trench/ForwardTaintEnvironment.h>
#include <mariana-trench/MethodContext.h>

namespace marianatrench {

class ForwardTaintTransfer final : public InstructionAnalyzerBase<
                                       ForwardTaintTransfer,
                                       ForwardTaintEnvironment,
                                       MethodContext*> {
 public:
  static bool analyze_default(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_check_cast(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_iget(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_sget(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_invoke(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_iput(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_sput(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_load_param(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_move(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_move_result(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_aget(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_aput(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_new_array(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_filled_new_array(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_unop(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_binop(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_binop_lit(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_const_string(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);

  static bool analyze_return(
      MethodContext* context,
      const IRInstruction* instruction,
      ForwardTaintEnvironment* taint);
};

} // namespace marianatrench
