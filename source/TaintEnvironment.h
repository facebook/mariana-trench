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
#include <mariana-trench/PointsToEnvironment.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

class TaintEnvironment final : public sparta::AbstractDomain<TaintEnvironment> {
 private:
  using Map =
      sparta::PatriciaTreeMapAbstractPartition<RootMemoryLocation*, TaintTree>;

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

  const TaintTree& get(RootMemoryLocation* root_memory_location) const {
    return environment_.get(root_memory_location);
  }

  void set(RootMemoryLocation* root_memory_location, TaintTree tree) {
    environment_.set(root_memory_location, std::move(tree));
  }

  template <typename Operation> // TaintTree(const TaintTree&)
  void update(RootMemoryLocation* root_memory_location, Operation&& operation) {
    static_assert(std::is_same_v<
                  decltype(operation(std::declval<TaintTree&&>())),
                  TaintTree>);
    environment_.update(
        root_memory_location, std::forward<Operation>(operation));
  }

  TaintTree read(MemoryLocation* memory_location) const;

  TaintTree read(MemoryLocation* memory_location, const Path& path) const;

  TaintTree read(const MemoryLocationsDomain& memory_locations) const;

  TaintTree read(
      const MemoryLocationsDomain& memory_locations,
      const Path& path) const;

  void write(
      MemoryLocation* memory_location,
      const Path& path,
      TaintTree taint,
      UpdateKind kind);

  void write(
      const MemoryLocationsDomain& memory_locations,
      const Path& path,
      TaintTree taint,
      UpdateKind kind);

  /**
   * Build a complete taint tree for the given memory location.
   * This merges all the taint trees reachable from the given memory location
   * via the resolved aliases points-to tree.
   */
  TaintTree deep_read(
      const ResolvedAliasesMap& resolved_aliases,
      MemoryLocation* memory_location) const;

  /**
   * Build a complete taint tree for the given memory locations domain.
   * This joins all the deep read taint trees in the memory locations domain.
   */
  TaintTree deep_read(
      const ResolvedAliasesMap& resolved_aliases,
      const MemoryLocationsDomain& memory_locations) const;

  /**
   * Read the specified path from the complete taint tree built for the given
   * memory locations domain.
   */
  TaintTree deep_read(
      const ResolvedAliasesMap& resolved_aliases,
      const MemoryLocationsDomain& memory_locations,
      const Path& path) const;

  /**
   * Write the given taint tree at the deepest memory locations reachable from
   * the root. The deepest memory location is found using the resolved aliases
   * points-to tree.
   */
  void deep_write(
      const ResolvedAliasesMap& resolved_aliases,
      MemoryLocation* memory_location,
      const Path& path,
      TaintTree taint,
      UpdateKind kind);

  /**
   * Write the given taint tree at the deepest memory locations reachable from
   * the roots in the memory locations domain. The deepest memory location is
   * found using the resolved aliases points-to tree.
   */
  void deep_write(
      const ResolvedAliasesMap& resolved_aliases,
      const MemoryLocationsDomain& memory_locations,
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
