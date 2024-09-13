/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <DexClass.h>

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
};

} // namespace marianatrench
