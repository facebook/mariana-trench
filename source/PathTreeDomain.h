/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/SingletonAbstractDomain.h>

namespace marianatrench {

struct PathTreeConfiguration {
  static std::size_t max_tree_height_after_widening() {
    return Heuristics::kPropagationOutputPathTreeWideningHeight;
  }

  static SingletonAbstractDomain transform_on_widening_collapse(
      SingletonAbstractDomain value) {
    return value;
  }
};

using PathTreeDomain =
    AbstractTreeDomain<SingletonAbstractDomain, PathTreeConfiguration>;

} // namespace marianatrench
