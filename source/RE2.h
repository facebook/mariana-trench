/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <re2/re2.h>

#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

/**
 * If the regular expression is equivalent to an equality check, return the
 * string literal, otherwise return std::nullopt.
 *
 * For instance:
 * ```
 * >>> as_string_literal(re2::RE2("Foo"))
 * <<< std::optional<std::string>("Foo")
 * >>> as_string_literal(re2::RE2("Foo.*"))
 * <<< std::nullopt
 * ```
 */
std::optional<std::string> as_string_literal(const re2::RE2& pattern);

/**
 * Return a substring that every string matching the regular expression must
 * contain, or std::nullopt if no such substring could be derived.
 *
 * This is a conservative approximation: a result is only returned when the
 * substring is provably required. Callers may therefore use its absence to
 * rule out a match, but never to conclude one.
 *
 * For instance:
 * ```
 * >>> required_substring(re2::RE2(".*okhttp3/Request;\\.post:.*"))
 * <<< std::optional<std::string>("okhttp3/Request;.post:")
 * >>> required_substring(re2::RE2("get(Eager|Multibind)"))
 * <<< std::nullopt
 * ```
 */
std::optional<std::string> required_substring(const re2::RE2& pattern);

/**
 * A regular expression together with a literal substring that any matching
 * string must contain, used to skip the regular expression altogether when the
 * substring is absent.
 *
 * This matters for scalability rather than for single-threaded speed: re2
 * guards each pattern's lazily-built DFA state cache with a reader-writer lock
 * held for the duration of every match. Model generators match a handful of
 * patterns against millions of methods across all cores at once, so that lock
 * becomes a single contended cache line and dominates the run. Answering with
 * a substring search keeps the common (non-matching) case lock-free.
 */
class MatchPattern final {
 public:
  explicit MatchPattern(const std::string& regex_string);

  // Matches `re2::RE2`, which is neither copyable nor movable.
  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(MatchPattern)

  bool full_match(std::string_view text) const;
  bool partial_match(std::string_view text) const;

  const std::string& pattern() const {
    return regular_expression_.pattern();
  }

  const re2::RE2& regular_expression() const {
    return regular_expression_;
  }

 private:
  re2::RE2 regular_expression_;
  // Empty when no required substring could be derived.
  std::string required_substring_;
};

} // namespace marianatrench
