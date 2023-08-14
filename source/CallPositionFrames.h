/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <initializer_list>
#include <ostream>

#include <boost/iterator/transform_iterator.hpp>
#include <json/json.h>

#include <sparta/AbstractDomain.h>
#include <sparta/PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/CalleePortFrames.h>
#include <mariana-trench/FlattenIterator.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/FramesMap.h>
#include <mariana-trench/GroupHashedSetAbstractDomain.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/RootPatriciaTreeAbstractPartition.h>
#include <mariana-trench/TaintConfig.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

class CallPositionProperties {
 public:
  explicit CallPositionProperties(const Position* MT_NULLABLE position);

  explicit CallPositionProperties(const TaintConfig& config);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CallPositionProperties)

  bool operator==(const CallPositionProperties& other) const {
    return position_ == other.position_;
  }

  static CallPositionProperties make_default() {
    return CallPositionProperties(/* position */ nullptr);
  }

  bool is_default() const {
    return position_ == nullptr;
  }

  void set_to_default() {
    position_ = nullptr;
  }

  const Position* MT_NULLABLE position() const {
    return position_;
  }

 private:
  const Position* MT_NULLABLE position_;
};

struct CalleePortFromTaintConfig {
  AccessPath operator()(const TaintConfig& config) const;
};

/**
 * Represents a set of frames with the same call position.
 * Based on its position in `Taint`, it is expected that all frames within
 * this class have the same callee and call position.
 */
class CallPositionFrames final : public FramesMap<
                                     CallPositionFrames,
                                     AccessPath,
                                     CalleePortFrames,
                                     CalleePortFromTaintConfig,
                                     CallPositionProperties> {
 private:
  using Base = FramesMap<
      CallPositionFrames,
      AccessPath,
      CalleePortFrames,
      CalleePortFromTaintConfig,
      CallPositionProperties>;

 public:
  INCLUDE_DERIVED_FRAMES_MAP_CONSTRUCTORS(
      CallPositionFrames,
      Base,
      CallPositionProperties)

  const Position* MT_NULLABLE position() const {
    return properties_.position();
  }

  FeatureMayAlwaysSet locally_inferred_features(
      const AccessPath& callee_port) const;

  void add_local_position(const Position* position);

  /**
   * Propagate the taint from the callee to the caller.
   *
   * Return bottom if the taint should not be propagated.
   */
  CallPositionFrames propagate(
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments,
      const CallClassIntervalContext& class_interval_context,
      const ClassIntervals::Interval& caller_class_interval) const;

  /* Return the set of leaf frames with the given position. */
  CallPositionFrames attach_position(const Position* position) const;

  CallPositionFrames apply_transform(
      const KindFactory& kind_factory,
      const TransformsFactory& transforms_factory,
      const UsedKinds& used_kinds,
      const TransformList* local_transforms) const;

  CallPositionFrames update_with_propagation_trace(
      const Frame& propagation_frame) const;

  /**
   * Returns new `CallPositionFrames` containing updated call and local
   * positions computed by the input functions.
   */
  std::unordered_map<const Position*, CallPositionFrames> map_positions(
      const std::function<const Position*(const AccessPath&, const Position*)>&
          new_call_position,
      const std::function<LocalPositionSet(const LocalPositionSet&)>&
          new_local_positions) const;

  Json::Value to_json(
      const Method* MT_NULLABLE callee,
      CallInfo call_info,
      ExportOriginsMode export_origins_mode) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const CallPositionFrames& frames);
};

} // namespace marianatrench
