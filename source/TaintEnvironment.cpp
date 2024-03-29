/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/TaintEnvironment.h>

namespace marianatrench {

std::ostream& operator<<(std::ostream& out, const TaintEnvironment& taint) {
  if (taint.is_bottom()) {
    return out << "_|_";
  } else if (taint.is_top()) {
    return out << "T";
  } else {
    out << "TaintEnvironment(";
    for (const auto& entry : taint.bindings()) {
      out << "\n  " << show(entry.first) << " -> " << entry.second;
    }
    return out << "\n)";
  }
}

} // namespace marianatrench
