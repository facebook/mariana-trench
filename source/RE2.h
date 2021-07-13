/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <string>

#include <re2/re2.h>

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

} // namespace marianatrench
