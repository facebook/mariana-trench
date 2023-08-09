/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/CallPositionFrames.h>
#include <mariana-trench/FramesMap.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/Position.h>
#include <mariana-trench/TaintConfig.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

class CalleeProperties {
 public:
  explicit CalleeProperties(
      const Method* MT_NULLABLE callee,
      CallInfo call_info);

  explicit CalleeProperties(const TaintConfig& config);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CalleeProperties)

  bool operator==(const CalleeProperties& other) const;

  static CalleeProperties make_default() {
    return CalleeProperties(/* callee */ nullptr, CallInfo::declaration());
  }

  bool is_default() const;

  void set_to_default();

  const Method* MT_NULLABLE callee() const {
    return callee_;
  }

  CallInfo call_info() const {
    return call_info_;
  }

 private:
  const Method* MT_NULLABLE callee_;
  CallInfo call_info_;
};

struct CallPositionFromTaintConfig {
  const Position* MT_NULLABLE operator()(const TaintConfig& config) const;
};

class CalleeFrames final : public FramesMap<
                               CalleeFrames,
                               const Position * MT_NULLABLE,
                               CallPositionFrames,
                               CallPositionFromTaintConfig,
                               CalleeProperties> {
 private:
  using Base = FramesMap<
      CalleeFrames,
      const Position * MT_NULLABLE,
      CallPositionFrames,
      CallPositionFromTaintConfig,
      CalleeProperties>;

 public:
  INCLUDE_DERIVED_FRAMES_MAP_CONSTRUCTORS(CalleeFrames, Base, CalleeProperties)

  struct GroupEqual {
    bool operator()(const CalleeFrames& left, const CalleeFrames& right) const {
      return left.callee() == right.callee() &&
          left.call_info() == right.call_info();
    }
  };

  struct GroupHash {
    std::size_t operator()(const CalleeFrames& frame) const {
      std::size_t seed = 0;
      boost::hash_combine(seed, frame.callee());
      boost::hash_combine(seed, frame.call_info().encode());
      return seed;
    }
  };

  struct GroupDifference {
    void operator()(CalleeFrames& left, const CalleeFrames& right) const {
      left.difference_with(right);
    }
  };

  const Method* MT_NULLABLE callee() const {
    return properties_.callee();
  }

  CallInfo call_info() const {
    return properties_.call_info();
  }

  void add_local_position(const Position* position);

  FeatureMayAlwaysSet locally_inferred_features(
      const Position* MT_NULLABLE position,
      const AccessPath& callee_port) const;

  /**
   * Propagate the taint from the callee to the caller.
   *
   * Return bottom if the taint should not be propagated.
   */
  CalleeFrames propagate(
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments)
      const;

  /**
   * Propagate the taint from the callee to the caller to track the next hops
   * for taints with CallInfo kind PropagationWithTrace.
   */
  CalleeFrames update_with_propagation_trace(
      const Frame& propagation_frame) const;

  /* Return the set of leaf frames with the given position. */
  CalleeFrames attach_position(const Position* position) const;

  CalleeFrames apply_transform(
      const KindFactory& kind_factory,
      const TransformsFactory& transforms_factory,
      const UsedKinds& used_kinds,
      const TransformList* local_transforms) const;

  void append_to_propagation_output_paths(Path::Element path_element);

  void update_maximum_collapse_depth(CollapseDepth collapse_depth);

  void update_non_leaf_positions(
      const std::function<
          const Position*(const Method*, const AccessPath&, const Position*)>&
          new_call_position,
      const std::function<LocalPositionSet(const LocalPositionSet&)>&
          new_local_positions);

  Json::Value to_json(ExportOriginsMode export_origins_mode) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const CalleeFrames& frames);
};

} // namespace marianatrench
