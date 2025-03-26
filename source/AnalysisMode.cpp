/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdexcept>

#include <fmt/format.h>

#include <mariana-trench/AnalysisMode.h>
#include <mariana-trench/Assert.h>

namespace marianatrench {

AnalysisMode analysis_mode_from_string(std::string_view value) {
  if (value == "normal") {
    return AnalysisMode::Normal;
  } else if (value == "cached_models") {
    return AnalysisMode::CachedModels;
  } else if (value == "replay") {
    return AnalysisMode::Replay;
  }

  throw std::invalid_argument(fmt::format(
      "Invalid analysis mode: {}. Expected one of: normal|cached_models|replay",
      value));
}

std::string analysis_mode_to_string(AnalysisMode mode) {
  switch (mode) {
    case AnalysisMode::Normal:
      return "normal";
    case AnalysisMode::CachedModels:
      return "cached_models";
    case AnalysisMode::Replay:
      return "replay";
    default:
      mt_unreachable();
  }
}

} // namespace marianatrench
