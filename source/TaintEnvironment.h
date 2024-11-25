/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

class TaintEnvironment final : public sparta::AbstractDomain<TaintEnvironment> {
 private:
  using Map =
      sparta::PatriciaTreeMapAbstractPartition<MemoryLocation*, TaintTree>;

 public:
  /* Create a bottom environment */
  TaintEnvironment() : environment_(Map::bottom()) {}

 private:
  explicit TaintEnvironment(Map environment)
      : environment_(std::move(environment)) {}

 public:
  explicit TaintEnvironment(
      std::initializer_list<std::pair<MemoryLocation*, TaintTree>> l) {
    for (const auto& p : l) {
      set(p.first, p.second);
    }
  }

  INCLUDE_ABSTRACT_DOMAIN_METHODS(TaintEnvironment, Map, environment_)

  const TaintTree& get(MemoryLocation* root_memory_location) const {
    // TaintTree's are only stored at root memory locations.
    mt_assert(root_memory_location->root() == root_memory_location);
    return environment_.get(root_memory_location);
  }

  void set(MemoryLocation* root_memory_location, const TaintTree& tree) {
    mt_assert(root_memory_location->root() == root_memory_location);
    environment_.set(root_memory_location, tree);
  }

  template <typename Operation> // TaintTree(const TaintTree&)
  void update(MemoryLocation* root_memory_location, Operation&& operation) {
    static_assert(std::is_same_v<
                  decltype(operation(std::declval<TaintTree&&>())),
                  TaintTree>);
    mt_assert(root_memory_location->root() == root_memory_location);
    environment_.update(
        root_memory_location, std::forward<Operation>(operation));
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const marianatrench::TaintEnvironment& taint);

 private:
  Map environment_;
};

} // namespace marianatrench
