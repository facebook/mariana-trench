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

#include <AbstractDomain.h>

#include <mariana-trench/CalleeFrames.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/GroupHashedSetAbstractDomain.h>
#include <mariana-trench/TaintConfig.h>

namespace marianatrench {

class TaintFramesIterator;

/**
 * Represents an abstract taint, as a map from taint kind to set of frames.
 * Replacement of `Taint`.
 */
class Taint final : public sparta::AbstractDomain<Taint> {
 private:
  struct GroupEqual {
    bool operator()(const CalleeFrames& left, const CalleeFrames& right) const {
      return left.callee() == right.callee();
    }
  };

  struct GroupHash {
    std::size_t operator()(const CalleeFrames& frame) const {
      return std::hash<const Method*>()(frame.callee());
    }
  };

  struct GroupDifference {
    void operator()(CalleeFrames& left, const CalleeFrames& right) const {
      left.difference_with(right);
    }
  };

  using Set = GroupHashedSetAbstractDomain<
      CalleeFrames,
      GroupHash,
      GroupEqual,
      GroupDifference>;

  friend class TaintFramesIterator;

 public:
  /* Create the bottom (i.e, empty) taint. */
  Taint() = default;

  explicit Taint(std::initializer_list<TaintConfig> configs);

  Taint(const Taint&) = default;
  Taint(Taint&&) = default;
  Taint& operator=(const Taint&) = default;
  Taint& operator=(Taint&&) = default;

  static Taint bottom() {
    return Taint();
  }

  static Taint top() {
    mt_unreachable(); // Not implemented.
  }

  bool is_bottom() const override {
    return set_.is_bottom();
  }

  bool is_top() const override {
    return set_.is_top();
  }

  void set_to_bottom() override {
    set_.set_to_bottom();
  }

  void set_to_top() override {
    set_.set_to_top();
  }

  bool empty() const {
    return set_.empty();
  }

  TaintFramesIterator frames_iterator() const;

  /**
   * Uses `frames_iterator()` to compute number of frames. This iterates over
   * every frame and can be expensive. Use for testing only.
   */
  std::size_t num_frames() const;

  void add(const TaintConfig& config);

  void clear() {
    set_.clear();
  }

  bool leq(const Taint& other) const override;

  bool equals(const Taint& other) const override;

  void join_with(const Taint& other) override;

  void widen_with(const Taint& other) override;

  void meet_with(const Taint& other) override;

  void narrow_with(const Taint& other) override;

  void difference_with(const Taint& other);

  /**
   * Sets the origins for leaves that do not have one set yet.
   * For use when instantiating the `Model` of a method, once the concrete
   * method (i.e. the origin of the `Taint`) becomes known.
   */
  void set_leaf_origins_if_empty(const MethodSet& origins);

  /**
   * Similar to `set_leaf_origins_if_empty` but also sets the field callee.
   * For use when instantiating `FieldModel` once the concrete field is known.
   * Reason this sets an additional `field_callee` is because field callees
   * are tracked separately from method callees.
   *
   * It is expected that this method is only ever called on leaves, i.e.
   * [method]callee == nullptr, because `FieldModel`s are always leaves. There
   * is no field-to-field taint propagation.
   */
  void set_field_origins_if_empty_with_field_callee(const Field* field);

  void add_inferred_features(const FeatureMayAlwaysSet& features);

  void add_local_position(const Position* position);

  void set_local_positions(const LocalPositionSet& positions);

  LocalPositionSet local_positions() const;

  void add_inferred_features_and_local_position(
      const FeatureMayAlwaysSet& features,
      const Position* MT_NULLABLE position);

  /**
   * Propagate the taint from the callee to the caller.
   *
   * Return bottom if the taint should not be propagated.
   */
  Taint propagate(
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      const FeatureMayAlwaysSet& extra_features,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments)
      const;

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
  void transform_kind_with_features(
      const std::function<std::vector<const Kind*>(const Kind*)>&,
      const std::function<FeatureMayAlwaysSet(const Kind*)>&);

  Json::Value to_json() const;

  friend std::ostream& operator<<(std::ostream& out, const Taint& taint);

  /**
   * Appends `path_element` to the callee ports of all artificial source frames
   * held in this instance.
   */
  void append_callee_port_to_artificial_sources(Path::Element path_element);

  /**
   * Adds `features` to the inferred features for all real (i.e not artificial)
   * sources held in this instance.
   */
  void add_inferred_features_to_real_sources(
      const FeatureMayAlwaysSet& features);

  /**
   * Update call and local positions of all non-leaf frames.
   * `new_call_position` is given callee, callee_port and (existing) position.
   * `new_local_positions` is given existing local positions.
   */
  void update_non_leaf_positions(
      const std::function<
          const Position*(const Method*, const AccessPath&, const Position*)>&
          new_call_position,
      const std::function<LocalPositionSet(const LocalPositionSet&)>&
          new_local_positions);

  /**
   * Drops frames that are considered invalid.
   * `is_valid` is given callee (nullptr for leaves), callee_port, kind.
   */
  void filter_invalid_frames(
      const std::function<
          bool(const Method* MT_NULLABLE, const AccessPath&, const Kind*)>&
          is_valid);

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
   * Similar to `partition_by_kind()` but caller gets to decide what value of
   * type `T` each kind maps to.
   */
  template <class T>
  std::unordered_map<T, Taint> partition_by_kind(
      const std::function<T(const Kind*)>& map_kind) const {
    std::unordered_map<T, Taint> result;
    for (const auto& callee_frames : set_) {
      auto callee_frames_partitioned =
          callee_frames.partition_by_kind(map_kind);

      for (const auto& [mapped_value, callee_frames] :
           callee_frames_partitioned) {
        auto existing = result.find(mapped_value);
        auto existing_or_bottom =
            existing == result.end() ? Taint::bottom() : existing->second;
        existing_or_bottom.add(callee_frames);

        result[mapped_value] = existing_or_bottom;
      }
    }
    return result;
  }

  /**
   * Returns a map from root to collapsed input paths for all the artificial
   * sources contained in this Taint instance.
   */
  RootPatriciaTreeAbstractPartition<PathTreeDomain> input_paths() const;

  /**
   * Returns all features for this taint tree, joined as `FeatureMayAlwaysSet`.
   */
  FeatureMayAlwaysSet features_joined() const;

  /**
   * Return an artificial source for the given access path.
   *
   * An artificial source is a source used to track the flow of a parameter,
   * to infer sinks and propagations. Instead of relying on a backward analysis,
   * we introduce these artificial sources in the forward analysis. This saves
   * the maintenance cost of having a forward and backward transfer function.
   */
  static Taint artificial_source(AccessPath access_path);

 private:
  void add(const CalleeFrames& frames);

  void map(const std::function<void(CalleeFrames&)>& f);

 private:
  Set set_;
};

class TaintFramesIterator {
 private:
  using ConstIterator = FlattenIterator<
      /* OuterIterator */ Taint::Set::iterator,
      /* InnerIterator */ CalleeFrames::iterator>;

 public:
  // C++ container concept member types
  using iterator = ConstIterator;
  using const_iterator = ConstIterator;
  using value_type = Frame;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const Frame&;
  using const_pointer = const Frame*;

 public:
  explicit TaintFramesIterator(const Taint& taint) : taint_(taint) {}

  const_iterator begin() const {
    return ConstIterator(taint_.set_.begin(), taint_.set_.end());
  }

  const_iterator end() const {
    return ConstIterator(taint_.set_.end(), taint_.set_.end());
  }

 private:
  const Taint& taint_;
};

} // namespace marianatrench
