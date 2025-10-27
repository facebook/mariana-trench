/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <initializer_list>
#include <ostream>

#include <json/json.h>

#include <sparta/AbstractDomain.h>
#include <sparta/PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/CallInfo.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/KindFrames.h>
#include <mariana-trench/RootPatriciaTreeAbstractPartition.h>
#include <mariana-trench/TaintConfig.h>

namespace marianatrench {

/**
 * Represents a set of frames with the same call info (callee, call kind,
 * callee port, position).
 */
class LocalTaint final : public sparta::AbstractDomain<LocalTaint> {
 private:
  using FramesByKind =
      sparta::PatriciaTreeMapAbstractPartition<const Kind*, KindFrames>;

 private:
  explicit LocalTaint(
      const CallInfo& call_info,
      FramesByKind frames,
      LocalPositionSet local_positions,
      FeatureMayAlwaysSet locally_inferred_features)
      : call_info_(call_info),
        frames_(std::move(frames)),
        local_positions_(std::move(local_positions)),
        locally_inferred_features_(std::move(locally_inferred_features)) {
    mt_assert(!local_positions_.is_bottom());

    // This constructor should NOT be used to create bottom.
    mt_assert(!frames_.is_bottom());
  }

 public:
  /**
   * Create the bottom (i.e, empty) local taint.
   *
   * We do not use `bottom()` for `local_positions_` because
   * `bottom().add(new_position)` gives `bottom()` which is not the desired
   * behavior for `LocalTaint::add`. Consider re-visiting LocalPositionSet.
   */
  LocalTaint()
      : call_info_(CallInfo::make_default()),
        frames_(FramesByKind::bottom()),
        local_positions_({}),
        locally_inferred_features_(FeatureMayAlwaysSet::bottom()) {}

  explicit LocalTaint(std::initializer_list<TaintConfig> configs);

  explicit LocalTaint(const Frame& frame);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(LocalTaint)

  static LocalTaint bottom() {
    return LocalTaint();
  }

  static LocalTaint top() {
    mt_unreachable();
  }

  bool is_bottom() const {
    auto is_bottom = frames_.is_bottom();
    if (is_bottom) {
      // Assert to ensure that set_to_bottom() is called whenever frames_ is
      // updated. Not strictly required for correct functionality, but helpful
      // to have a definitive notion of bottom.
      mt_assert(
          call_info_.is_default() && local_positions_.empty() &&
          locally_inferred_features_.is_bottom());
    }
    return is_bottom;
  }

  bool is_top() const {
    return frames_.is_top();
  }

  void set_to_bottom() {
    call_info_ = CallInfo::make_default();
    frames_.set_to_bottom();
    local_positions_ = {};
    locally_inferred_features_.set_to_bottom();
  }

  void set_to_top() {
    // This domain is never set to top.
    mt_unreachable();
  }

  bool empty() const {
    return frames_.is_bottom();
  }

  const CallInfo& call_info() const {
    return call_info_;
  }

  const Method* MT_NULLABLE callee() const {
    return call_info_.callee();
  }

  CallKind call_kind() const {
    return call_info_.call_kind();
  }

  const AccessPath* MT_NULLABLE callee_port() const {
    return call_info_.callee_port();
  }

  const Position* MT_NULLABLE call_position() const {
    return call_info_.call_position();
  }

  const LocalPositionSet& local_positions() const {
    return local_positions_;
  }

  const FeatureMayAlwaysSet& locally_inferred_features() const {
    return locally_inferred_features_;
  }

  void add(const TaintConfig& config);

  void add(const Frame& frame);

  bool leq(const LocalTaint& other) const;

  bool equals(const LocalTaint& other) const;

  void join_with(const LocalTaint& other);

  void widen_with(const LocalTaint& other);

  void meet_with(const LocalTaint& other);

  void narrow_with(const LocalTaint& other);

  void difference_with(const LocalTaint& other);

  template <typename Function> // KindFrames(KindFrames)
  void transform_kind_frames(Function&& f) {
    static_assert(
        std::is_same_v<decltype(f(std::declval<KindFrames&&>())), KindFrames>);
    frames_.transform(f);
    if (frames_.is_bottom()) {
      set_to_bottom();
    }
  }

  template <typename Function> // Frame(Frame)
  void transform_frames(Function&& f) {
    static_assert(std::is_same_v<decltype(f(std::declval<Frame&&>())), Frame>);
    frames_.transform([f = std::forward<Function>(f)](KindFrames kind_frames) {
      kind_frames.transform(f);
      return kind_frames;
    });
    if (frames_.is_bottom()) {
      set_to_bottom();
    }
  }

  template <typename Visitor> // void(const CallInfo&, const Frame&)
  void visit_frames(Visitor&& visitor) const {
    static_assert(
        std::is_void_v<decltype(visitor(
            std::declval<const CallInfo&>(), std::declval<const Frame&>()))>);

    frames_.visit(
        [visitor = std::forward<Visitor>(visitor), call_info = call_info_](
            const std::pair<const Kind*, KindFrames>& binding) {
          binding.second.visit([visitor, &call_info](const Frame& frame) {
            visitor(call_info, frame);
          });
        });
  }

  template <typename Visitor> // void(const KindFrames&)
  void visit_kind_frames(Visitor&& visitor) const {
    static_assert(
        std::is_void_v<decltype(visitor(std::declval<const KindFrames&>()))>);

    frames_.visit([visitor = std::forward<Visitor>(visitor)](
                      const std::pair<const Kind*, KindFrames>& binding) {
      visitor(binding.second);
    });
  }

  template <typename Predicate> // bool(const Frame&)
  void filter_frames(Predicate&& predicate) {
    static_assert(
        std::is_same_v<decltype(predicate(std::declval<const Frame>())), bool>);
    frames_.transform([predicate = std::forward<Predicate>(predicate)](
                          KindFrames kind_frames) {
      kind_frames.filter(predicate);
      return kind_frames;
    });
    if (frames_.is_bottom()) {
      set_to_bottom();
    }
  }

  void add_origins_if_declaration(const Method* method, const AccessPath* port);

  void add_origins_if_declaration(const Field* field);

  void add_origins_if_declaration(std::string_view literal);

  void add_locally_inferred_features(const FeatureMayAlwaysSet& features);

  void add_local_position(const Position* position);

  void add_local_positions(const LocalPositionSet& positions);

  void set_local_positions(LocalPositionSet positions);

  /**
   * Appends `path_element` to the output paths of all propagation frames.
   */
  void append_to_propagation_output_paths(Path::Element path_element);

  /**
   * Propagate the taint from the callee to the caller.
   *
   * Return bottom if the taint should not be propagated.
   */
  LocalTaint propagate(
      const Method* MT_NULLABLE callee,
      const AccessPath* MT_NULLABLE callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments,
      const CallClassIntervalContext& class_interval_context,
      const ClassIntervals::Interval& caller_class_interval,
      const RootPatriciaTreeAbstractPartition<FeatureSet>&
          add_features_to_arguments) const;

  /**
   * Propagate the taint from the callee to the caller to track the next hops
   * for taints with CallInfo kind PropagationWithTrace.
   */
  LocalTaint update_with_propagation_trace(
      const CallInfo& propagation_call_info,
      const Frame& propagation_frame) const;

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
              new_kind, [&new_frame](const KindFrames& existing) {
                return existing.join(new_frame);
              });
        }
      }
    }
    if (new_frames_by_kind.is_bottom()) {
      set_to_bottom();
    } else {
      frames_ = std::move(new_frames_by_kind);
    }
  }

  /* Return the set of leaf frames with the given position. */
  LocalTaint attach_position(const Position* call_position) const;

  LocalTaint apply_transform(
      const KindFactory& kind_factory,
      const TransformsFactory& transforms,
      const UsedKinds& used_kinds,
      const TransformList* local_transforms,
      transforms::TransformDirection direction) const;

  LocalTaint add_sanitize_transform(
      const Sanitizer& sanitizer,
      const KindFactory& kind_factory,
      const TransformsFactory& transforms_factory) const;

  void update_maximum_collapse_depth(CollapseDepth collapse_depth);

  std::vector<LocalTaint> update_non_declaration_positions(
      const std::function<const Position*(
          const Method*,
          const AccessPath* MT_NULLABLE,
          const Position* MT_NULLABLE)>& new_call_position,
      const std::function<LocalPositionSet(const LocalPositionSet&)>&
          new_local_positions) const;

  std::vector<LocalTaint> update_origin_positions(
      const std::function<const Position*(
          const Method*,
          const AccessPath* MT_NULLABLE,
          const Position* MT_NULLABLE)>& new_call_position,
      const LocalPositionSet& new_local_positions) const;

  void filter_invalid_frames(
      const std::function<bool(
          const Method* MT_NULLABLE,
          const AccessPath* MT_NULLABLE,
          const Kind*)>& is_valid);

  bool contains_kind(const Kind*) const;

  template <class T>
  std::unordered_map<T, LocalTaint> partition_by_kind(
      const std::function<T(const Kind*)>& map_kind) const {
    std::unordered_map<T, LocalTaint> result;

    for (const auto& [kind, frame] : frames_.bindings()) {
      T mapped_value = map_kind(kind);
      result[mapped_value].join_with(LocalTaint(
          call_info_,
          FramesByKind{std::pair(kind, frame)},
          local_positions_,
          locally_inferred_features_));
    }
    return result;
  }

  FeatureMayAlwaysSet features_joined() const;

  static LocalTaint from_json(const Json::Value& value, Context& context);
  Json::Value to_json(ExportOriginsMode export_origins_mode) const;

  friend std::ostream& operator<<(std::ostream& out, const LocalTaint& frames);

 private:
  CallInfo call_info_;
  FramesByKind frames_;
  LocalPositionSet local_positions_;
  FeatureMayAlwaysSet locally_inferred_features_;
};

} // namespace marianatrench
