/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <iostream>

namespace marianatrench {

/**
 * Helper to print sets (e.g. feature or annotation feature sets) in a unified
 * fashion.
 */
template<class Set>
std::ostream& show_set(std::ostream& out, const Set& set) {
  out << "{";
  for (auto iterator = set.begin(), end = set.end();
       iterator != end;) {
    out << "`" << show(*iterator) << "`";
    ++iterator;
    if (iterator != end) {
      out << ", ";
    }
  }
  return out << "}";
}

} // namespace marianatrench
