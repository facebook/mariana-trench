/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <DexClass.h>
#include <IRInstruction.h>

namespace marianatrench {

/**
 * KotlinHeuristics contains helper methods used to implement kotlin specific
 * logic during analysis. Eg, logic to handle specifics of
 * `kotlin/jvm/internal/`.
 */
class KotlinHeuristics final {
 public:
  /**
   * Check if we can skip creating parameter type overrides for the given
   * callee
   */
  static bool skip_parameter_type_overrides(const DexMethod* callee);

  /**
   * Check if callee is a known kotlin method without side effects
   */
  static bool method_has_side_effects(const DexMethod* callee);

  /**
   * Check if const-string instruction is a known kotlin generated string
   * without side effects
   */
  static bool const_string_has_side_effect(const IRInstruction* instruction);
};

} // namespace marianatrench
