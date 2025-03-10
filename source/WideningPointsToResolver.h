/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/PointsToEnvironment.h>

namespace marianatrench {

class WidenedPointsToComponents final {
 public:
  using Component = boost::container::flat_set<RootMemoryLocation*>;
  using ComponentsMap =
      boost::container::flat_map<RootMemoryLocation*, Component>;

 public:
  WidenedPointsToComponents() = default;

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(WidenedPointsToComponents)

  std::size_t size() const {
    return components_.size();
  }

  /**
   * Create a new widened component with the given head.
   */
  void create_component(RootMemoryLocation* head);

  /**
   * Add a member to the widened component with the given head.
   */
  void add_component(RootMemoryLocation* head, RootMemoryLocation* member);

  /**
   * If the given memory_location is a member of a widened component,
   * return the head. Otherwise, return null.
   */
  RootMemoryLocation* MT_NULLABLE
  get_head(RootMemoryLocation* memory_location) const;

  /**
   * If the given memory_location is a member of a widened component,
   * return it. Otherwise, return null.
   */
  const Component* MT_NULLABLE
  get_component(RootMemoryLocation* memory_location) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const WidenedPointsToComponents& components);

 private:
  ComponentsMap components_;
};

/** Mapping from RootMemoryLocation to a PointsToTree using a concise
 * representation.
 */
using RootMemoryLocationPointsToTreeMap =
    boost::container::flat_map<RootMemoryLocation*, PointsToTree>;

/**
 * Widening points-to resolver takes a points-to environment and applies
 * widening to it. It stores the resolved aliases for the environment which is
 * used to readout the points-to information.
 */
class WideningPointsToResolver final {
 public:
  explicit WideningPointsToResolver(const PointsToEnvironment& environment);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(WideningPointsToResolver)

  const WidenedPointsToComponents& widened_components() const {
    return widened_components_;
  }

  PointsToTree resolved_aliases(RootMemoryLocation* root_memory_location) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const WideningPointsToResolver& widening_resolver);

 private:
  RootMemoryLocationPointsToTreeMap widened_resolved_aliases_;
  WidenedPointsToComponents widened_components_;
};

} // namespace marianatrench
