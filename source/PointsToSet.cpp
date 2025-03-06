/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/PointsToSet.h>

namespace marianatrench {

PointsToSet::PointsToSet(RootMemoryLocation* memory_location) {
  set_internal(memory_location, AliasingProperties::empty());
}

PointsToSet::PointsToSet(
    RootMemoryLocation* memory_location,
    AliasingProperties properties) {
  set_internal(memory_location, std::move(properties));
}

PointsToSet::PointsToSet(
    std::initializer_list<RootMemoryLocation*> memory_locations) {
  for (auto* memory_location : memory_locations) {
    set_internal(memory_location, AliasingProperties::empty());
  }
}

PointsToSet::PointsToSet(
    std::initializer_list<std::pair<RootMemoryLocation*, AliasingProperties>>
        points_tos) {
  for (auto& points_to : points_tos) {
    set_internal(points_to.first, points_to.second);
  }
}

PointsToSet::PointsToSet(const MemoryLocationsDomain& memory_locations) {
  for (auto* memory_location : memory_locations) {
    auto* root_memory_location = memory_location->as<RootMemoryLocation>();
    mt_assert(root_memory_location != nullptr);
    set_internal(root_memory_location, AliasingProperties::empty());
  }
}

void PointsToSet::add_local_position(const Position* position) {
  map_.transform([position](AliasingProperties properties) {
    properties.add_local_position(position);
    return properties;
  });
}

void PointsToSet::add_locally_inferred_features(
    const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map_.transform([&features](AliasingProperties properties) {
    properties.add_locally_inferred_features(features);
    return properties;
  });
}

void PointsToSet::update_aliasing_properties(
    RootMemoryLocation* points_to,
    const AliasingProperties& properties) {
  map_.update(points_to, [&properties](const AliasingProperties& existing) {
    auto result = existing;
    result.join_with(properties);
    return result;
  });
}

PointsToSet PointsToSet::with_aliasing_properties(
    const AliasingProperties& new_properties) const {
  PointsToSet result{};

  for (const auto& [points_to, _properties] : map_.bindings()) {
    result.set_internal(points_to, new_properties);
  }

  return result;
}

std::ostream& operator<<(std::ostream& out, const PointsToSet& points_to_set) {
  mt_assert(!points_to_set.is_top());

  out << "PointsToSet{";
  for (const auto& [memory_location, properties] :
       points_to_set.map_.bindings()) {
    out << show(memory_location) << " -> " << properties << ", ";
  }

  return out << "}";
}

} // namespace marianatrench
