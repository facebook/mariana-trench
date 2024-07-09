/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/CollapseDepth.h>

namespace marianatrench {

struct PathTreeConfiguration {
  static std::size_t max_tree_height_after_widening() {
    return Heuristics::singleton()
        .propagation_output_path_tree_widening_height();
  }

  static CollapseDepth transform_on_widening_collapse(
      CollapseDepth /* depth */) {
    return CollapseDepth::zero();
  }

  static CollapseDepth transform_on_sink(CollapseDepth depth) {
    if (depth.is_zero()) {
      return CollapseDepth::zero();
    } else {
      return CollapseDepth::bottom();
    }
  }

  static CollapseDepth transform_on_hoist(CollapseDepth depth) {
    if (depth.is_bottom()) {
      return CollapseDepth::bottom();
    } else {
      return CollapseDepth::zero();
    }
  }
};

using PathTreeDomain = AbstractTreeDomain<CollapseDepth, PathTreeConfiguration>;

} // namespace marianatrench
