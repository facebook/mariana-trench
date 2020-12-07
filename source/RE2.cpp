/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <array>

#include <re2/re2.h>
#include <re2/regexp.h>

#include <mariana-trench/RE2.h>

namespace marianatrench {

std::optional<std::string> as_literal_string(const re2::RE2& pattern) {
  auto* regular_expression = pattern.Regexp();

  // Check if the regular expression tree is a literal string leaf.
  // `FoldCase` indicates that the regular expression is case insensitive.
  if (regular_expression->op() != re2::kRegexpLiteralString ||
      (regular_expression->parse_flags() & re2::Regexp::FoldCase) != 0) {
    return std::nullopt;
  }

  // Transcode from runes (UTF8 code-points) to bytes.
  std::string content;
  int runes = regular_expression->nrunes();
  for (int index = 0; index < runes; index++) {
    std::array<char, 4> bytes;
    int size =
        re2::runetochar(bytes.data(), &regular_expression->runes()[index]);
    content.append(bytes.data(), size);
  }

  return content;
}

} // namespace marianatrench
