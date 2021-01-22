/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Registry.h>

namespace marianatrench {

class Highlights {
 public:
  /*
   * The following struct and method have been included here only for testing
   * purposes
   */
  struct Bounds {
    int line;
    int start;
    int end;

    bool operator==(const Bounds& rhs) const {
      return (rhs.line == line) && (rhs.start == start) && (rhs.end == end);
    }
  };
  static Bounds get_highlight_bounds(
      const Method* callee,
      const std::vector<std::string>& lines,
      int callee_line_number,
      const AccessPath& callee_port);

  /*
   * Add a start and end column to the positions involved in issues so that they
   * can be highlighted in the Zoncolan UI
   */
  static void augment_positions(Registry&, const Context& context);
};

} // namespace marianatrench
