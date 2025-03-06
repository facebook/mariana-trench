/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/AbstractDomain.h>
#include <sparta/PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/AliasingProperties.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/MemoryLocationEnvironment.h>

namespace marianatrench {

class PointsToSet final : public sparta::AbstractDomain<PointsToSet> {
 private:
  using Map = sparta::
      PatriciaTreeMapAbstractPartition<RootMemoryLocation*, AliasingProperties>;

 public:
  // C++ container concept member types
  using key_type = RootMemoryLocation*;
  using mapped_type = AliasingProperties;
  using value_type = std::pair<const RootMemoryLocation*, AliasingProperties>;
  using iterator = typename Map::MapType::iterator;
  using const_iterator = iterator;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const value_type&;
  using const_pointer = const value_type*;

 private:
  explicit PointsToSet(Map map) : map_(std::move(map)) {}

 public:
  PointsToSet() : map_(Map::bottom()) {}

  explicit PointsToSet(RootMemoryLocation* memory_location);

  explicit PointsToSet(
      RootMemoryLocation* memory_location,
      AliasingProperties properties);

  explicit PointsToSet(
      std::initializer_list<RootMemoryLocation*> memory_locations);

  explicit PointsToSet(
      std::initializer_list<std::pair<RootMemoryLocation*, AliasingProperties>>
          points_tos);

  explicit PointsToSet(const MemoryLocationsDomain& memory_locations);

  INCLUDE_ABSTRACT_DOMAIN_METHODS(PointsToSet, Map, map_)

  void difference_with(const PointsToSet& other) {
    if (other.is_bottom()) {
      return;
    }
    map_.difference_like_operation(
        other.map_,
        [](const AliasingProperties& left, const AliasingProperties& right) {
          if (left.leq(right)) {
            return AliasingProperties::empty();
          }

          return left;
        });
  }

  std::size_t size() const {
    return map_.size();
  }

  iterator begin() const {
    return map_.bindings().begin();
  }

  iterator end() const {
    return map_.bindings().end();
  }

  void add_local_position(const Position* position);

  void add_locally_inferred_features(const FeatureMayAlwaysSet& features);

  void update_aliasing_properties(
      RootMemoryLocation* points_to,
      const AliasingProperties& properties);

  /**
   * Return a copy of the points-to set with the given aliasing properties.
   */
  PointsToSet with_aliasing_properties(
      const AliasingProperties& properties) const;

 private:
  void set_internal(
      RootMemoryLocation* memory_location,
      AliasingProperties properties) {
    mt_assert(!properties.is_top());
    map_.set(memory_location, std::move(properties));
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const PointsToSet& points_to_set);

 private:
  Map map_;
};

} // namespace marianatrench
