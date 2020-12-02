// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <InstructionAnalyzer.h>

#include <mariana-trench/AnalysisEnvironment.h>
#include <mariana-trench/MethodContext.h>

namespace marianatrench {

class Transfer final : public InstructionAnalyzerBase<
                           Transfer,
                           AnalysisEnvironment,
                           MethodContext*> {
 public:
  static bool analyze_default(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_check_cast(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_iget(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_invoke(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_iput(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_load_param(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_move(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_move_result(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_aget(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_aput(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_new_array(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_filled_new_array(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_unop(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_binop(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_binop_lit(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);

  static bool analyze_return(
      MethodContext* context,
      const IRInstruction* instruction,
      AnalysisEnvironment* taint);
};

} // namespace marianatrench
