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

/* A substring shorter than this filters too little to pay for itself. */
constexpr std::size_t k_minimum_required_substring_length = 3;

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

std::optional<std::string> required_substring(
    const re2::RE2& regular_expression) {
  if (!regular_expression.ok()) {
    return std::nullopt;
  }

  const auto& pattern = regular_expression.pattern();
  std::string longest;
  std::string current;

  auto close_run = [&longest, &current]() {
    if (current.size() > longest.size()) {
      longest = current;
    }
    current.clear();
  };

  for (std::size_t index = 0; index < pattern.size(); index++) {
    char byte = pattern[index];
    std::optional<char> literal;

    if (byte == '\\') {
      if (index + 1 >= pattern.size() || !is_escapable(pattern[index + 1])) {
        // An escape we do not model, e.g. `\d`, `\x41`, `\Q..\E` or a trailing
        // backslash. We cannot tell how many of the bytes that follow belong to
        // the escape, so treating them as literals would be unsound.
        return std::nullopt;
      }
      literal = pattern[++index];
    } else if (byte == '|' || byte == '(' || byte == '[' || byte == '{') {
      // Alternation, grouping, character classes and repetition counts can all
      // make an otherwise-literal run optional. Give up rather than reason
      // about them.
      return std::nullopt;
    } else if (is_literal(byte)) {
      literal = byte;
    }

    char quantifier = index + 1 < pattern.size() ? pattern[index + 1] : '\0';
    bool optional = quantifier == '?' || quantifier == '*';
    bool repeated = quantifier == '+';
    if (optional || repeated) {
      index++;
    }

    if (!literal || optional) {
      close_run();
      continue;
    }

    current.push_back(*literal);
    if (repeated) {
      // `a+` guarantees at least one `a`, but anything after it need not be
      // adjacent to what came before.
      close_run();
    }
  }
  close_run();

  if (longest.size() < k_minimum_required_substring_length) {
    return std::nullopt;
  }
  return longest;
}

MatchPattern::MatchPattern(const std::string& regex_string)
    : regular_expression_(regex_string),
      required_substring_(
          required_substring(regular_expression_).value_or(std::string())) {}

bool MatchPattern::full_match(std::string_view text) const {
  if (!required_substring_.empty() &&
      text.find(required_substring_) == std::string_view::npos) {
    return false;
  }
  return re2::RE2::FullMatch(text, regular_expression_);
}

bool MatchPattern::partial_match(std::string_view text) const {
  if (!required_substring_.empty() &&
      text.find(required_substring_) == std::string_view::npos) {
    return false;
  }
  return re2::RE2::PartialMatch(text, regular_expression_);
}

} // namespace marianatrench
