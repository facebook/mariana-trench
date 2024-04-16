/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <initializer_list>
#include <ostream>

#include <boost/functional/hash.hpp>
#include <json/json.h>

#include <sparta/AbstractDomain.h>
#include <sparta/PatriciaTreeHashMapAbstractPartition.h>

#include <mariana-trench/CallInfo.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/LocalTaint.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/PropagationConfig.h>
#include <mariana-trench/TaintConfig.h>

namespace marianatrench {

/**
 * Represents an abstract taint, as a map from taint kind to set of frames.
 */
class Taint final : public sparta::AbstractDomain<Taint> {
 private:
  using Map =
      sparta::PatriciaTreeHashMapAbstractPartition<CallInfo, LocalTaint>;

  explicit Taint(Map map) : map_(std::move(map)) {}

 public:
  /* Create the bottom (i.e, empty) taint. */
  Taint() = default;

  explicit Taint(std::initializer_list<TaintConfig> configs);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Taint)

  INCLUDE_ABSTRACT_DOMAIN_METHODS(Taint, Map, map_)

  bool empty() const {
    return map_.is_bottom();
  }

  /**
   * This iterates over every frame and can be expensive. Use for testing only.
   */
  std::size_t num_frames() const;

  void add(const TaintConfig& config);

  void add(const LocalTaint& local_taint);

  void clear() {
    map_.set_to_bottom();
  }

  void difference_with(const Taint& other);

  template <typename Function> // Frame(Frame)
  void transform_frames(Function&& f) {
    static_assert(std::is_same_v<decltype(f(std::declval<Frame&&>())), Frame>);

    map_.transform(
        [f = std::forward<Function>(f)](LocalTaint* local_taint) -> void {
          local_taint->transform_frames(f);
        });
  }

  template <typename Visitor> // void(const CallInfo&, const Frame&)
  void visit_frames(Visitor&& visitor) const {
    static_assert(
        std::is_void_v<decltype(visitor(
            std::declval<const CallInfo&>(), std::declval<const Frame&>()))>);

    map_.visit([visitor = std::forward<Visitor>(visitor)](
                   const std::pair<CallInfo, LocalTaint>& binding) {
      binding.second.visit_frames(visitor);
    });
  }

  template <typename Visitor> // void(const KindFrames&)
  void visit_kind_frames(Visitor&& visitor) const {
    static_assert(
        std::is_void_v<decltype(visitor(std::declval<const KindFrames&>()))>);

    map_.visit([visitor = std::forward<Visitor>(visitor)](
                   const std::pair<CallInfo, LocalTaint>& binding) {
      binding.second.visit_kind_frames(visitor);
    });
  }

  template <typename Visitor> // void(const LocalTaint&)
  void visit_local_taint(Visitor&& visitor) const {
    static_assert(
        std::is_void_v<decltype(visitor(std::declval<const LocalTaint&>()))>);

    map_.visit([visitor = std::forward<Visitor>(visitor)](
                   const std::pair<CallInfo, LocalTaint>& binding) {
      visitor(binding.second);
    });
  }

  template <typename Predicate> // bool(const Frame&)
  void filter_frames(Predicate&& predicate) {
    static_assert(
        std::is_same_v<decltype(predicate(std::declval<const Frame>())), bool>);

    map_.transform(
        [predicate = std::forward<Predicate>(predicate)](
            LocalTaint* local_taint) -> void {
          local_taint->filter_frames(predicate);
        });
  }

  /**
   * For use when instantiating the `Model` of a method, once the concrete
   * method (i.e. the origin of the `Taint`) becomes known.
   */
  void add_origins_if_declaration(const Method* method, const AccessPath* port);

  /**
   * For use when instantiating `FieldModel` once the concrete field is known.
   *
   * It is expected that this method is only ever called on leaves, i.e.
   * [method]callee == nullptr, because `FieldModel`s are always leaves. There
   * is no field-to-field taint propagation.
   */
  void add_origins_if_declaration(const Field* field);

  /**
   * For use when instantiating `LiteralModel` when the literal is known.
   */
  void add_origins_if_declaration(std::string_view literal);

  void add_locally_inferred_features(const FeatureMayAlwaysSet& features);

  void add_local_position(const Position* position);

  void set_local_positions(const LocalPositionSet& positions);

  LocalPositionSet local_positions() const;

  FeatureMayAlwaysSet locally_inferred_features(
      const CallInfo& call_info) const;

  void add_locally_inferred_features_and_local_position(
      const FeatureMayAlwaysSet& features,
      const Position* MT_NULLABLE position);

  /**
   * Propagate the taint from the callee to the caller.
   *
   * Return bottom if the taint should not be propagated.
   */
  Taint propagate(
      const Method* MT_NULLABLE callee,
      const AccessPath* MT_NULLABLE callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments,
      const CallClassIntervalContext& class_interval_context,
      const ClassIntervals::Interval& caller_class_interval) const;

  /* Return the set of leaf frames with the given position. */
  Taint attach_position(const Position* position) const;

  /**
   * Transforms kinds in the taint according to the function in the first arg.
   * Returning an empty vec will cause frames for the input kind to be dropped.
   * If a transformation occurs (returns more than a vector containing just the
   * input kind), locally inferred features can be added to the frames of the
   * transformed kinds (return `bottom()` to add nothing).
   *
   * If multiple kinds map to the same kind, their respective frames will be
   * joined. This means "always" features could turn into "may" features. At
   * time of writing, there should be no such use-case, but new callers should
   * be mindful of this behavior.
   */
  template <typename TransformKind, typename AddFeatures>
  void transform_kind_with_features(
      TransformKind&& transform_kind, // std::vector<const Kind*>(const Kind*)
      AddFeatures&& add_features // FeatureMayAlwaysSet(const Kind*)
  ) {
    map_.transform(
        [transform_kind = std::forward<TransformKind>(transform_kind),
         add_features](LocalTaint* local_taint) -> void {
          local_taint->transform_kind_with_features(
              transform_kind, add_features);
        });
  }

  Taint apply_transform(
      const KindFactory& kind_factory,
      const TransformsFactory& transforms_factory,
      const UsedKinds& used_kinds,
      const TransformList* local_transforms) const;

  static Taint from_json(const Json::Value& taint_json, Context& context);
  Json::Value to_json(ExportOriginsMode export_origins_mode) const;

  friend std::ostream& operator<<(std::ostream& out, const Taint& taint);

  /**
   * Appends `path_element` to the output paths of all propagation frames.
   */
  void append_to_propagation_output_paths(Path::Element path_element);

  void update_maximum_collapse_depth(CollapseDepth collapse_depth);

  Taint update_with_propagation_trace(
      const CallInfo& propagation_call_info,
      const Frame& propagation_frame) const;

  /**
   * Update call and local positions of all non-leaf frames.
   * `new_call_position` is given callee, callee_port and (existing) position.
   * `new_local_positions` is given existing local positions.
   */
  Taint update_non_declaration_positions(
      const std::function<const Position*(
          const Method*,
          const AccessPath* MT_NULLABLE,
          const Position* MT_NULLABLE)>& new_call_position,
      const std::function<LocalPositionSet(const LocalPositionSet&)>&
          new_local_positions) const;

  /**
   * Drops frames that are considered invalid.
   * `is_valid` is given callee (nullptr for leaves), callee_port, kind.
   */
  void filter_invalid_frames(const std::function<bool(
                                 const Method* MT_NULLABLE,
                                 const AccessPath* MT_NULLABLE,
                                 const Kind*)>& is_valid);

  /**
   * Returns true if any frame contains the given kind.
   */
  bool contains_kind(const Kind*) const;

  /**
   * Returns a map of `Kind` -> `Taint`, where each `Taint` value contains only
   * the frames with the `Kind` in its key.
   */
  std::unordered_map<const Kind*, Taint> partition_by_kind() const;

  /**
   * Similar to `partition_by_kind()` but the result is a vector sorted by the
   * string representation of the Kind.
   */
  std::vector<std::pair<const Kind*, Taint>> sorted_partition_by_kind() const;

  /**
   * Similar to `partition_by_kind()` but caller gets to decide what value of
   * type `Key` each kind maps to.
   */
  template <typename Key>
  std::unordered_map<Key, Taint> partition_by_kind(
      const std::function<Key(const Kind*)>& map_kind) const {
    std::unordered_map<Key, Taint> result;
    for (const auto& [_, local_taint] : map_.bindings()) {
      auto local_taint_partitioned = local_taint.partition_by_kind(map_kind);

      for (const auto& [mapped_value, local_taint] : local_taint_partitioned) {
        result[mapped_value].add(local_taint);
      }
    }
    return result;
  }

  template <typename Key>
  std::unordered_map<Key, Taint> partition_by_call_kind(
      const std::function<Key(CallKind)>& map_call_kind) const {
    std::unordered_map<Key, Taint> result;
    for (const auto& [_, local_taint] : map_.bindings()) {
      auto mapped_value = map_call_kind(local_taint.call_kind());
      result[mapped_value].add(local_taint);
    }
    return result;
  }

  /**
   * Retain only intervals that intersect with `other`. This happens regardless
   * of kind, i.e. intervals will be dropped even if kind is not the same.
   */
  void intersect_intervals_with(const Taint& other);

  /**
   * Returns all features for this taint tree, joined as `FeatureMayAlwaysSet`.
   */
  FeatureMayAlwaysSet features_joined() const;

  /**
   * Returns all kinds used.
   */
  std::unordered_set<const Kind*> kinds() const;

  /**
   * Return the taint representing the given propagation.
   */
  static Taint propagation(PropagationConfig propagation);

  /**
   * Create the taint used to infer propagations in the backward analysis.
   */
  static Taint propagation_taint(
      const PropagationKind* kind,
      PathTreeDomain output_paths,
      FeatureMayAlwaysSet inferred_features,
      FeatureSet user_features);

  /**
   * Return the same taint without any non-essential information (e.g,
   * features).
   *
   * This is used to create a mold for `AccessPathTreeDomain::shape_with`.
   */
  Taint essential() const;

 private:
  Map map_;
};

} // namespace marianatrench
