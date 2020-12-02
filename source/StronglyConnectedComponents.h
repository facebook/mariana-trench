// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vector>

#include <mariana-trench/Dependencies.h>
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

  StronglyConnectedComponents(const StronglyConnectedComponents&) = default;
  StronglyConnectedComponents(StronglyConnectedComponents&&) = default;
  StronglyConnectedComponents& operator=(const StronglyConnectedComponents&) =
      default;
  StronglyConnectedComponents& operator=(StronglyConnectedComponents&&) =
      default;
  ~StronglyConnectedComponents() = default;

  const std::vector<std::vector<const Method*>>& components() const {
    return components_;
  }

 private:
  std::vector<std::vector<const Method*>> components_;
};

} // namespace marianatrench
