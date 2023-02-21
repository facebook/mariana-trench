/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>

#include <mariana-trench/Dependencies.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>

namespace marianatrench {

/*
 * Compute strongly connected components of a graph.
 *
 * The strongly connected components are in reverse topological order (from
 * leaves to roots).
 */
class StronglyConnectedComponents final {
 public:
  explicit StronglyConnectedComponents(
      const Methods& methods,
      const Dependencies& dependencies);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(StronglyConnectedComponents)

  const std::vector<std::vector<const Method*>>& components() const {
    return components_;
  }

 private:
  std::vector<std::vector<const Method*>> components_;
};

} // namespace marianatrench
