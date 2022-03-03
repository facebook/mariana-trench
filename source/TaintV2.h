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

namespace marianatrench {

/**
 * Represents an abstract taint, as a map from taint kind to set of frames.
 * Replacement of `Taint`.
 */
class TaintV2 final : public sparta::AbstractDomain<TaintV2> {
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

 public:
  /* Create the bottom (i.e, empty) taint. */
  TaintV2() = default;

  explicit TaintV2(std::initializer_list<Frame> frames);

  TaintV2(const TaintV2&) = default;
  TaintV2(TaintV2&&) = default;
  TaintV2& operator=(const TaintV2&) = default;
  TaintV2& operator=(TaintV2&&) = default;

  static TaintV2 bottom() {
    return TaintV2();
  }

  static TaintV2 top() {
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

  std::size_t size() const {
    return set_.size();
  }

  bool empty() const {
    return set_.empty();
  }

  void add(const Frame& frame);

  void clear() {
    set_.clear();
  }

  bool leq(const TaintV2& other) const override;

  bool equals(const TaintV2& other) const override;

  void join_with(const TaintV2& other) override;

  void widen_with(const TaintV2& other) override;

  void meet_with(const TaintV2& other) override;

  void narrow_with(const TaintV2& other) override;

  void difference_with(const TaintV2& other);

  void add_inferred_features(const FeatureMayAlwaysSet& features);

  void add_local_position(const Position* position);

  void set_local_positions(const LocalPositionSet& positions);

  void add_inferred_features_and_local_position(
      const FeatureMayAlwaysSet& features,
      const Position* MT_NULLABLE position);

  /**
   * Propagate the taint from the callee to the caller.
   *
   * Return bottom if the taint should not be propagated.
   */
  TaintV2 propagate(
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
  TaintV2 attach_position(const Position* position) const;

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
  TaintV2 transform_kind_with_features(
      const std::function<std::vector<const Kind*>(const Kind*)>&,
      const std::function<FeatureMayAlwaysSet(const Kind*)>&) const;

  /**
   * Appends `path_element` to the callee ports of all kind that pass `filter`
   * (returns true).
   */
  void append_callee_port(
      Path::Element path_element,
      const std::function<bool(const Kind*)>& filter);

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
  std::unordered_map<const Kind*, TaintV2> partition_by_kind() const;

  /**
   * Returns all features for this taint tree, joined as `FeatureMayAlwaysSet`.
   */
  FeatureMayAlwaysSet features_joined() const;

 private:
  void add(const CalleeFrames& frames);

  void map(const std::function<void(CalleeFrames&)>& f);

 private:
  Set set_;
};

} // namespace marianatrench
