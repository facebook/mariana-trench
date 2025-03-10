/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/AliasingProperties.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/MemoryLocationEnvironment.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/PointsToTree.h>

namespace marianatrench {

class WideningPointsToResolver;

class PointsToEnvironment final
    : public sparta::AbstractDomain<PointsToEnvironment> {
 private:
  using Map = sparta::
      PatriciaTreeMapAbstractPartition<RootMemoryLocation*, PointsToTree>;

 public:
  using key_type = RootMemoryLocation*;
  using mapped_type = PointsToTree;
  using value_type = std::pair<const RootMemoryLocation*, PointsToTree>;
  using iterator = typename Map::MapType::iterator;
  using const_iterator = iterator;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const value_type&;
  using const_pointer = const value_type*;

 public:
  /* Create a bottom environment */
  PointsToEnvironment() : environment_(Map::bottom()) {}

  explicit PointsToEnvironment(Map environment)
      : environment_(std::move(environment)) {}

  explicit PointsToEnvironment(
      std::initializer_list<std::pair<RootMemoryLocation*, PointsToTree>> l) {
    for (const auto& p : l) {
      set(p.first, p.second);
    }
  }

  INCLUDE_ABSTRACT_DOMAIN_METHODS(PointsToEnvironment, Map, environment_)

  iterator begin() const {
    return environment_.bindings().begin();
  }

  iterator end() const {
    return environment_.bindings().end();
  }

  const PointsToTree& get(RootMemoryLocation* root_memory_location) const {
    return environment_.get(root_memory_location);
  }

  void set(RootMemoryLocation* root_memory_location, PointsToTree tree) {
    environment_.set(root_memory_location, std::move(tree));
  }

  template <typename Operation> // PointsToTree(const PointsToTree&)
  void update(RootMemoryLocation* root_memory_location, Operation&& operation) {
    static_assert(std::is_same_v<
                  decltype(operation(std::declval<PointsToTree&&>())),
                  PointsToTree>);
    environment_.update(
        root_memory_location, std::forward<Operation>(operation));
  }

  /**
   * Creates a widening resolver reflecting the current state of the points-to
   * environment.
   */
  WideningPointsToResolver make_widening_resolver() const;

  /**
   * Resolve all possible aliases for the points-to tree at the given
   * root_memory_location.
   *
   * Expands all the memory locations to their corresponding points-to trees in
   * the environment and builds a single points-to tree.
   */
  PointsToTree resolve_aliases(RootMemoryLocation* root_memory_location) const;

 private:
  void resolve_aliases_internal(
      RootMemoryLocation* memory_location,
      const Path& path,
      const AliasingProperties& properties,
      PointsToTree& resolved_aliases,
      std::unordered_set<MemoryLocation*>& visited) const;

 public:
  /**
   * Resolve the alias for a given memory location.
   *
   * If the memory location is a root memory location, resolves to itself.
   * Otherwise, it is a field memory location and resolves to the points-to set
   * in the deepest node in the points-to environment.
   *
   * This differs from resolve_aliases in that it only expands the points-to
   * tree along the path of the field memory location and hence is not a
   * complete resolution.
   */
  PointsToSet points_to(MemoryLocation* memory_location) const;

  /**
   * Resolve the alias for a all memory locations in the given memory locations
   * domain.
   */
  PointsToSet points_to(const MemoryLocationsDomain& memory_locations) const;

  /**
   * Create an alias from memory location at path field to the points_tos set.
   *
   * Writes the points_tos set at the deeps node in the points-to environment.
   */
  void write(
      MemoryLocation* memory_location,
      const DexString* field,
      const PointsToSet& points_tos,
      UpdateKind kind);

  /**
   * Create an alias from memory location at path field to the points_tos set.
   *
   * Writes the points_tos set at the deeps node in the points-to environment.
   */
  void write(
      const WideningPointsToResolver& widening_resolver,
      MemoryLocation* memory_location,
      const DexString* field,
      const PointsToSet& points_tos,
      UpdateKind kind);

  friend std::ostream& operator<<(
      std::ostream& out,
      const marianatrench::PointsToEnvironment& taint);

 private:
  Map environment_;
};

/**
 * Mapping from root memory locations to the resolved points-to tree using
 * a concise representation.
 */
class ResolvedAliasesMap final {
 private:
  using Map = boost::container::flat_map<RootMemoryLocation*, PointsToTree>;

 public:
  using iterator = typename Map::const_iterator;
  using const_iterator = iterator;
  using key_type = Map::key_type;
  using mapped_type = Map::value_type;
  using value_type = std::pair<const key_type, mapped_type>;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const value_type&;
  using const_pointer = const value_type*;

 private:
  explicit ResolvedAliasesMap(Map map) : map_(std::move(map)) {}

 public:
  /**
   * Delete default constructor so that only way to create this is using the
   * named constructors.
   */
  ResolvedAliasesMap() = delete;

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ResolvedAliasesMap)

  iterator begin() const {
    return map_.begin();
  }

  iterator end() const {
    return map_.end();
  }

  /**
   * Returns the resolved points-to tree for the given root memory location.
   */
  PointsToTree get(RootMemoryLocation* root_memory_location) const;

  /**
   * Builds the map of resolved points-to trees for the root memory locations
   * used by the instruction.
   */
  static ResolvedAliasesMap from_environments(
      const Method* method,
      MemoryFactory& memory_factory,
      const MemoryLocationEnvironment& memory_locations_environment,
      const PointsToEnvironment& points_to_environment,
      const IRInstruction* instruction);

  friend std::ostream& operator<<(std::ostream& out, const ResolvedAliasesMap&);

 private:
  Map map_;
};

} // namespace marianatrench
