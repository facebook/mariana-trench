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

 private:
  void add(const CalleeFrames& frames);

  void map(const std::function<void(CalleeFrames&)>& f);

 private:
  Set set_;
};

} // namespace marianatrench
