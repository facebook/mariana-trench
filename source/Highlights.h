/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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
   * Add a start and end column to the positions involved in issues so that they
   * can be highlighted in the Zoncolan UI
   */
  static void augment_positions(Registry&, const Context& context);

  /*
   * The following have been included here only for testing
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

  /*
   * Representation of the lines in a file. Used to prevent off-by-1 errors as
   * lines in files are 1-indexed while cpp vectors are 0-indexed
   */
  class FileLines {
   public:
    explicit FileLines(const std::vector<std::string>& lines) : lines_(lines) {}
    explicit FileLines(std::ifstream& stream);

    bool has_line_number(std::size_t index) const;

    const std::string& line(std::size_t index) const;

    std::size_t size() const;

   private:
    std::vector<std::string> lines_;
  };

  static Bounds get_callee_highlight_bounds(
      const DexMethod* callee,
      const FileLines& lines,
      int callee_line_number,
      const AccessPath& callee_port);

  /*
   * If there are multiple overlapping local positions on a line, choose the one
   * with the shortest highlight
   */
  static LocalPositionSet filter_overlapping_highlights(
      const LocalPositionSet& local_positions);

  static Bounds get_local_position_bounds(
      const Position& local_position,
      const FileLines& lines);
};

} // namespace marianatrench
