/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <string_view>

#include <re2/re2.h>

#include <mariana-trench/RE2.h>

namespace marianatrench {

namespace {

// We could use `std::isalnum`, but it takes into account locales and UTF8,
// which we don't care about here.
bool is_alphanumeric(char byte) {
  return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
      (byte >= '0' && byte <= '9');
}

/* Return true if the given byte doesn't have a special meaning in a regular
 * expression. */
bool is_literal(char byte) {
  const std::string_view safe_bytes = "!\"#%&',-/:;<=>@_`~";
  return is_alphanumeric(byte) || safe_bytes.find(byte) != std::string::npos;
}

/* Return true if the given byte can be safely escaped. */
bool is_escapable(char byte) {
  const std::string_view escapable_bytes = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
  return escapable_bytes.find(byte) != std::string::npos;
}

} // namespace

std::optional<std::string> as_string_literal(
    const re2::RE2& regular_expression) {
  if (!regular_expression.ok()) {
    return std::nullopt;
  }

  const auto& pattern = regular_expression.pattern();
  std::string result;
  result.reserve(pattern.size());

  for (std::size_t index = 0; index < pattern.size(); index++) {
    if (is_literal(pattern[index])) {
      result.push_back(pattern[index]);
    } else if (
        pattern[index] == '\\' && index + 1 < pattern.size() &&
        is_escapable(pattern[index + 1])) {
      index++;
      result.push_back(pattern[index]);
    } else {
      return std::nullopt;
    }
  }
  return result;
}

} // namespace marianatrench
