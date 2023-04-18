/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/AccessPathTreeDomain.h>
#include <mariana-trench/Taint.h>

namespace marianatrench {

struct TaintTreeConfiguration {
  static std::size_t max_tree_height_after_widening() {
    return Heuristics::kSourceSinkTreeWideningHeight;
  }

  static Taint transform_on_widening_collapse(Taint);

  static Taint transform_on_sink(Taint taint) {
    return taint;
  }

  static Taint transform_on_hoist(Taint taint) {
    return taint;
  }
};

using TaintTree = AbstractTreeDomain<Taint, TaintTreeConfiguration>;
using TaintAccessPathTree = AccessPathTreeDomain<Taint, TaintTreeConfiguration>;

} // namespace marianatrench
