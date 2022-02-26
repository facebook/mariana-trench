/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <AbstractDomain.h>
#include <PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/CallPositionFrames.h>
#include <mariana-trench/Position.h>

namespace marianatrench {

/**
 * Represents a set of frames with the same call position.
 * Based on its position in `Taint`, it is expected that all frames within
 * this class have the same callee and call position.
 */
class CalleeFrames final : public sparta::AbstractDomain<CalleeFrames> {
 private:
  using FramesByCallPosition = sparta::PatriciaTreeMapAbstractPartition<
      const Position * MT_NULLABLE,
      CallPositionFrames>;

 private:
  explicit CalleeFrames(
      const Method* MT_NULLABLE callee,
      FramesByCallPosition frames)
      : callee_(callee), frames_(std::move(frames)) {}

 public:
  /* Create the bottom (i.e, empty) frame set. */
  CalleeFrames() : callee_(nullptr), frames_(FramesByCallPosition::bottom()) {}

  explicit CalleeFrames(std::initializer_list<Frame> frames);

  CalleeFrames(const CalleeFrames&) = default;
  CalleeFrames(CalleeFrames&&) = default;
  CalleeFrames& operator=(const CalleeFrames&) = default;
  CalleeFrames& operator=(CalleeFrames&&) = default;

  static CalleeFrames bottom() {
    return CalleeFrames(
        /* callee */ nullptr, FramesByCallPosition::bottom());
  }

  static CalleeFrames top() {
    return CalleeFrames(
        /* callee */ nullptr, FramesByCallPosition::top());
  }

  bool is_bottom() const override {
    return frames_.is_bottom();
  }

  bool is_top() const override {
    return frames_.is_top();
  }

  void set_to_bottom() override {
    callee_ = nullptr;
    frames_.set_to_bottom();
  }

  void set_to_top() override {
    callee_ = nullptr;
    frames_.set_to_top();
  }

  bool empty() const {
    return frames_.is_bottom();
  }

  const Method* MT_NULLABLE callee() const {
    return callee_;
  }

  void add(const Frame& frame);

  bool leq(const CalleeFrames& other) const override;

  bool equals(const CalleeFrames& other) const override;

  void join_with(const CalleeFrames& other) override;

  void widen_with(const CalleeFrames& other) override;

  void meet_with(const CalleeFrames& other) override;

  void narrow_with(const CalleeFrames& other) override;

  void difference_with(const CalleeFrames& other);

  void map(const std::function<void(Frame&)>& f);

  void add_inferred_features(const FeatureMayAlwaysSet& features);

  LocalPositionSet local_positions() const;

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
  CalleeFrames propagate(
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments)
      const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const CalleeFrames& frames);

 private:
  const Method* MT_NULLABLE callee_;
  FramesByCallPosition frames_;
};

} // namespace marianatrench
