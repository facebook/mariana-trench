/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string_view>

namespace marianatrench {

enum class AnalysisMode : unsigned int {
  // The default, most frequently used mode.
  Normal = 0,
  // Analyze with cached models from a separate run, provided using
  // --sharded-models-directory.
  CachedModels = 1,
  // Replay a previous run for quick debugging. This avoids having to re-compute
  // various things and re-running the fixpoint from scratch. The previous run's
  // output should be provided using --sharded-models-directory.
  Replay = 2,
};

AnalysisMode analysis_mode_from_string(std::string_view value);

} // namespace marianatrench
