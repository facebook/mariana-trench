/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <vector>

namespace marianatrench {
namespace constants {

struct dfa_annotation {
  std::string type;
  std::string pattern_type;
};

dfa_annotation get_dfa_annotation();

std::vector<std::string> get_private_uri_schemes();

std::string_view get_privacy_decision_type();

/**
 * Determines which JSON format to use in the models' output.
 * `true` for compatibility with the older sapp-cli (will be deprecated).
 * `false` for a newer, more compact format.
 */
constexpr bool k_is_legacy_output_version = true;

} // namespace constants
} // namespace marianatrench
