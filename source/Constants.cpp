/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Constants.h>

namespace marianatrench {
namespace constants {

#ifndef MARIANA_TRENCH_FACEBOOK_BUILD
dfa_annotation get_dfa_annotation() {
  return {
      /* type */ "<undefined>",
      /* pattern_type */ "<undefined>",
  };
}

std::vector<std::string> get_private_uri_schemes() {
  return {};
}

std::string_view get_privacy_decision_type() {
  return "<undefined>";
}
#endif

} // namespace constants
} // namespace marianatrench
