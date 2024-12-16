/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/AbstractTaintTree.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/MemoryLocation.h>

namespace marianatrench {

class TaintEnvironment final : public sparta::AbstractDomain<TaintEnvironment> {
 private:
  using Map = sparta::
      PatriciaTreeMapAbstractPartition<RootMemoryLocation*, AbstractTaintTree>;

 public:
  /* Create a bottom environment */
  TaintEnvironment() : environment_(Map::bottom()) {}

 private:
  explicit TaintEnvironment(Map environment)
      : environment_(std::move(environment)) {}

 public:
  explicit TaintEnvironment(
      std::initializer_list<std::pair<RootMemoryLocation*, TaintTree>> l) {
    for (const auto& p : l) {
      set(p.first, p.second);
    }
  }

  INCLUDE_ABSTRACT_DOMAIN_METHODS(TaintEnvironment, Map, environment_)

  const AbstractTaintTree& get(RootMemoryLocation* root_memory_location) const {
    // TaintTree's are only stored at root memory locations.
    return environment_.get(root_memory_location);
  }

  /** Helper that wraps the TaintTree as AbstractTaintTree for now. */
  void set(RootMemoryLocation* root_memory_location, TaintTree tree) {
    environment_.set(root_memory_location, AbstractTaintTree{std::move(tree)});
  }

  template <typename Operation> // AbstractTaintTree(const AbstractTaintTree&)
  void update(RootMemoryLocation* root_memory_location, Operation&& operation) {
    static_assert(std::is_same_v<
                  decltype(operation(std::declval<TaintTree&&>())),
                  AbstractTaintTree>);
    environment_.update(
        root_memory_location, std::forward<Operation>(operation));
  }

  /** Helper that writes to the TaintTree only for now. */
  void write(
      MemoryLocation* memory_location,
      const Path& path,
      Taint taint,
      UpdateKind kind);

  /** Helper that writes to the TaintTree only for now. */
  void write(
      MemoryLocation* memory_location,
      const Path& path,
      TaintTree taint,
      UpdateKind kind);

  friend std::ostream& operator<<(
      std::ostream& out,
      const marianatrench::TaintEnvironment& taint);

 private:
  Map environment_;
};

} // namespace marianatrench
