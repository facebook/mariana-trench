/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/TaintEnvironment.h>

namespace marianatrench {

void TaintEnvironment::write(
    MemoryLocation* memory_location,
    const Path& path,
    Taint taint,
    UpdateKind kind) {
  Path full_path = memory_location->path();
  full_path.extend(path);

  environment_.update(
      memory_location->root(),
      [&full_path, &taint, kind](const AbstractTaintTree& tree) {
        auto copy = tree;
        copy.write_taint_tree(full_path, TaintTree{std::move(taint)}, kind);
        return copy;
      });
}

void TaintEnvironment::write(
    MemoryLocation* memory_location,
    const Path& path,
    TaintTree taint,
    UpdateKind kind) {
  Path full_path = memory_location->path();
  full_path.extend(path);

  environment_.update(
      memory_location->root(),
      [&full_path, &taint, kind](const AbstractTaintTree& tree) {
        auto copy = tree;
        copy.write_taint_tree(full_path, std::move(taint), kind);
        return copy;
      });
}

std::ostream& operator<<(
    std::ostream& out,
    const TaintEnvironment& environment) {
  if (environment.is_bottom()) {
    return out << "_|_";
  } else if (environment.is_top()) {
    return out << "T";
  } else {
    out << "TaintEnvironment(";
    for (const auto& entry : environment.environment_.bindings()) {
      out << "\n  " << show(entry.first) << " -> " << entry.second;
    }
    return out << "\n)";
  }
}

} // namespace marianatrench
