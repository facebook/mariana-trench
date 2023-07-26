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

#include <AbstractDomain.h>
#include <PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/TaintConfig.h>

namespace marianatrench {

/**
 * Represents a set of frames with the same call position.
 * Based on its position in `Taint`, it is expected that all frames within
 * this class have the same callee and call position.
 */
class CalleePortFrames final : public sparta::AbstractDomain<CalleePortFrames> {
 private:
  using FramesByKind =
      sparta::PatriciaTreeMapAbstractPartition<const Kind*, Frame>;

 private:
  struct GetFrameReference {
    const Frame& operator()(
        const std::pair<const Kind*, Frame>& element) const {
      return std::cref(element.second);
    }
  };

  using ConstIterator = boost::transform_iterator<
      GetFrameReference,
      typename FramesByKind::MapType::const_iterator>;

 public:
  // C++ container concept member types
  using iterator = ConstIterator;
  using const_iterator = ConstIterator;
  using value_type = Frame;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const Frame&;
  using const_pointer = const Frame*;

 private:
  explicit CalleePortFrames(
      AccessPath callee_port,
      FramesByKind frames,
      LocalPositionSet local_positions,
      FeatureMayAlwaysSet locally_inferred_features)
      : callee_port_(std::move(callee_port)),
        frames_(std::move(frames)),
        local_positions_(std::move(local_positions)),
        locally_inferred_features_(std::move(locally_inferred_features)) {
    mt_assert(!local_positions_.is_bottom());
  }

 public:
  /**
   * Create the bottom (i.e, empty) frame set. Value of callee_port_ doesn't
   * matter, so we pick some default (Leaf). Also avoid using `bottom()` for
   * local_positions_ because `bottom().add(new_position)` gives `bottom()`
   * which is not the desired behavior for `CalleePortFrames::add`.
   * Consider re-visiting LocalPositionSet.
   */
  CalleePortFrames()
      : callee_port_(Root(Root::Kind::Leaf)),
        frames_(FramesByKind::bottom()),
        local_positions_({}),
        locally_inferred_features_(FeatureMayAlwaysSet::bottom()) {}

  explicit CalleePortFrames(std::initializer_list<TaintConfig> configs);

  explicit CalleePortFrames(const Frame& frame);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CalleePortFrames)

  static CalleePortFrames bottom() {
    return CalleePortFrames();
  }

  static CalleePortFrames top() {
    return CalleePortFrames(
        /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
        FramesByKind::top(),
        /* local_positions */ {},
        /* locally_inferred_features */ FeatureMayAlwaysSet::top());
  }

  bool is_bottom() const {
    return frames_.is_bottom();
  }

  bool is_top() const {
    return frames_.is_top();
  }

  void set_to_bottom() {
    callee_port_ = AccessPath(Root(Root::Kind::Leaf));
    frames_.set_to_bottom();
    local_positions_ = {};
    locally_inferred_features_.set_to_bottom();
  }

  void set_to_top() {
    callee_port_ = AccessPath(Root(Root::Kind::Leaf));
    frames_.set_to_top();
    local_positions_.set_to_top();
    locally_inferred_features_.set_to_top();
  }

  bool empty() const {
    return frames_.is_bottom();
  }

  const AccessPath& callee_port() const {
    return callee_port_;
  }

  const LocalPositionSet& local_positions() const {
    return local_positions_;
  }

  const FeatureMayAlwaysSet& locally_inferred_features() const {
    return locally_inferred_features_;
  }

  void add(const TaintConfig& config);

  void add(const Frame& frame);

  bool leq(const CalleePortFrames& other) const;

  bool equals(const CalleePortFrames& other) const;

  void join_with(const CalleePortFrames& other);

  void widen_with(const CalleePortFrames& other);

  void meet_with(const CalleePortFrames& other);

  void narrow_with(const CalleePortFrames& other);

  void difference_with(const CalleePortFrames& other);

  template <typename Function> // Frame(Frame)
  void map(Function&& f) {
    static_assert(std::is_same_v<decltype(f(std::declval<Frame&&>())), Frame>);
    frames_.map(f);
  }

  template <typename Predicate> // bool(const Frame&)
  void filter(Predicate&& predicate) {
    static_assert(
        std::is_same_v<decltype(predicate(std::declval<const Frame>())), bool>);
    frames_.map([predicate = std::forward<Predicate>(predicate)](Frame frame) {
      if (!predicate(frame)) {
        return Frame::bottom();
      }
      return frame;
    });
  }

  ConstIterator begin() const {
    return boost::make_transform_iterator(
        frames_.bindings().begin(), GetFrameReference());
  }

  ConstIterator end() const {
    return boost::make_transform_iterator(
        frames_.bindings().end(), GetFrameReference());
  }

  void set_origins_if_empty(const MethodSet& origins);

  void set_field_origins_if_empty_with_field_callee(const Field* field);

  void add_locally_inferred_features(const FeatureMayAlwaysSet& features);

  void add_local_position(const Position* position);

  void set_local_positions(LocalPositionSet positions);

  void add_locally_inferred_features_and_local_position(
      const FeatureMayAlwaysSet& features,
      const Position* MT_NULLABLE position);

  /**
   * Appends `path_element` to the output paths of all propagation frames.
   */
  void append_to_propagation_output_paths(Path::Element path_element);

  /**
   * Propagate the taint from the callee to the caller.
   *
   * Return bottom if the taint should not be propagated.
   */
  CalleePortFrames propagate(
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments)
      const;

  template <typename TransformKind, typename AddFeatures>
  void transform_kind_with_features(
      TransformKind&& transform_kind, // std::vector<const Kind*>(const Kind*)
      AddFeatures&& add_features // FeatureMayAlwaysSet(const Kind*)
  ) {
    FramesByKind new_frames_by_kind;
    for (const auto& [old_kind, frame] : frames_.bindings()) {
      std::vector<const Kind*> new_kinds = transform_kind(old_kind);
      if (new_kinds.empty()) {
        continue;
      } else if (new_kinds.size() == 1 && new_kinds.front() == old_kind) {
        new_frames_by_kind.set(old_kind, frame); // no transformation
      } else {
        for (const auto* new_kind : new_kinds) {
          // Even if new_kind == old_kind for some new_kind, perform
          // map_frame_set because a transformation occurred.
          FeatureMayAlwaysSet features_to_add = add_features(new_kind);
          auto new_frame = frame.with_kind(new_kind);
          new_frame.add_inferred_features(features_to_add);
          new_frames_by_kind.update(
              new_kind, [&new_frame](const Frame& existing) {
                return existing.join(new_frame);
              });
        }
      }
    }
    frames_ = std::move(new_frames_by_kind);
    if (frames_.is_bottom()) {
      set_to_bottom();
    }
  }

  CalleePortFrames apply_transform(
      const KindFactory& kind_factory,
      const TransformsFactory& transforms,
      const UsedKinds& used_kinds,
      const TransformList* local_transforms) const;

  void filter_invalid_frames(
      const std::function<
          bool(const Method* MT_NULLABLE, const AccessPath&, const Kind*)>&
          is_valid);

  bool contains_kind(const Kind*) const;

  template <class T>
  std::unordered_map<T, CalleePortFrames> partition_by_kind(
      const std::function<T(const Kind*)>& map_kind) const {
    std::unordered_map<T, CalleePortFrames> result;

    for (const auto& [kind, frame] : frames_.bindings()) {
      T mapped_value = map_kind(kind);
      result[mapped_value].join_with(CalleePortFrames(
          callee_port_,
          FramesByKind{std::pair(kind, frame)},
          local_positions_,
          locally_inferred_features_));
    }
    return result;
  }

  FeatureMayAlwaysSet features_joined() const;

  template <class T>
  std::unordered_map<T, std::vector<std::reference_wrapper<const Frame>>>
  partition_map(const std::function<T(const Frame&)>& map) const {
    std::unordered_map<T, std::vector<std::reference_wrapper<const Frame>>>
        result;
    for (const auto& [_, frame] : frames_.bindings()) {
      auto value = map(frame);
      result[value].push_back(std::cref(frame));
    }

    return result;
  }

  Json::Value to_json(
      const Method* MT_NULLABLE callee,
      const Position* MT_NULLABLE position,
      CallInfo call_info,
      ExportOriginsMode export_origins_mode) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const CalleePortFrames& frames);

 private:
  Frame propagate_frames(
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments,
      std::vector<std::reference_wrapper<const Frame>> frames,
      std::vector<const Feature*>& via_type_of_features_added) const;

  CalleePortFrames propagate_crtex_leaf_frames(
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      std::vector<std::reference_wrapper<const Frame>> frames) const;

  /**
   * Checks that this object and `other` have the same key. Abstract domain
   * operations here only operate on `CalleePortFrames` that have the same key.
   * The only exception is if one of them `is_bottom()`.
   */
  bool has_same_key(const CalleePortFrames& other) const {
    return callee_port_ == other.callee_port();
  }

 private:
  AccessPath callee_port_;
  FramesByKind frames_;

  LocalPositionSet local_positions_;

  FeatureMayAlwaysSet locally_inferred_features_;
};

} // namespace marianatrench
