/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <AbstractDomain.h>
#include <PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/GroupHashedSetAbstractDomain.h>
#include <mariana-trench/TaintConfig.h>

namespace marianatrench {

/**
 * Represents a set of frames with the same call position.
 * Based on its position in `Taint`, it is expected that all frames within
 * this class have the same callee and call position.
 */
class CalleePortFramesV2 final
    : public sparta::AbstractDomain<CalleePortFramesV2> {
 private:
  using Frames =
      GroupHashedSetAbstractDomain<Frame, Frame::GroupHash, Frame::GroupEqual>;
  using FramesByKind =
      sparta::PatriciaTreeMapAbstractPartition<const Kind*, Frames>;

 private:
  explicit CalleePortFramesV2(
      AccessPath callee_port,
      bool is_result_or_receiver_sinks,
      FramesByKind frames,
      PathTreeDomain output_paths,
      LocalPositionSet local_positions)
      : callee_port_(std::move(callee_port)),
        frames_(std::move(frames)),
        is_result_or_receiver_sinks_(is_result_or_receiver_sinks),
        output_paths_(std::move(output_paths)),
        local_positions_(std::move(local_positions)) {
    mt_assert(!local_positions_.is_bottom());
    if (is_result_or_receiver_sinks_) {
      mt_assert(callee_port.path().empty());
    } else {
      mt_assert(output_paths_.is_bottom());
    }
  }

 public:
  /**
   * Create the bottom (i.e, empty) frame set. Value of callee_port_ and
   * is_result_or_receiver_sinks_ don't matter, so we pick some default. Leaf
   * and false respectively seem like decent choices.
   * Also avoid using `bottom()` for local_positions_ because
   * bottom().add(new_position) gives bottom() which is not the desired
   * behavior for CalleePortFrames::add. Consider re-visiting LocalPositionSet.
   */
  CalleePortFramesV2()
      : callee_port_(Root(Root::Kind::Leaf)),
        frames_(FramesByKind::bottom()),
        is_result_or_receiver_sinks_(false),
        output_paths_(PathTreeDomain::bottom()),
        local_positions_({}) {}

  explicit CalleePortFramesV2(std::initializer_list<TaintConfig> configs);

  CalleePortFramesV2(const CalleePortFramesV2&) = default;
  CalleePortFramesV2(CalleePortFramesV2&&) = default;
  CalleePortFramesV2& operator=(const CalleePortFramesV2&) = default;
  CalleePortFramesV2& operator=(CalleePortFramesV2&&) = default;

  static CalleePortFramesV2 bottom() {
    return CalleePortFramesV2();
  }

  static CalleePortFramesV2 top() {
    return CalleePortFramesV2(
        /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
        /* is_result_or_receiver_sinks */ false,
        FramesByKind::top(),
        /* output_paths */ {},
        /* local_positions */ {});
  }

  bool is_bottom() const override {
    return frames_.is_bottom();
  }

  bool is_top() const override {
    return frames_.is_top();
  }

  void set_to_bottom() override {
    callee_port_ = AccessPath(Root(Root::Kind::Leaf));
    is_result_or_receiver_sinks_ = false;
    frames_.set_to_bottom();
    output_paths_.set_to_bottom();
    local_positions_ = {};
  }

  void set_to_top() override {
    callee_port_ = AccessPath(Root(Root::Kind::Leaf));
    is_result_or_receiver_sinks_ = false;
    frames_.set_to_top();
    output_paths_.set_to_top();
    local_positions_.set_to_top();
  }

  bool empty() const {
    return frames_.is_bottom();
  }

  const AccessPath& callee_port() const {
    return callee_port_;
  }

  bool is_artificial_source_frames() const {
    mt_unreachable();
  }

  bool is_result_or_receiver_sinks() const {
    return is_result_or_receiver_sinks_;
  }

  const LocalPositionSet& local_positions() const {
    return local_positions_;
  }

  const PathTreeDomain& input_paths() const {
    mt_unreachable();
  }

  const PathTreeDomain& output_paths() const {
    return output_paths_;
  }

  void add(const TaintConfig& config);

  bool leq(const CalleePortFramesV2& other) const override;

  bool equals(const CalleePortFramesV2& other) const override;

  void join_with(const CalleePortFramesV2& other) override;

  void widen_with(const CalleePortFramesV2& other) override;

  void meet_with(const CalleePortFramesV2& other) override;

  void narrow_with(const CalleePortFramesV2& other) override;

  friend std::ostream& operator<<(
      std::ostream& out,
      const CalleePortFramesV2& frames);

 private:
  void add(const Frame& frame);

  /**
   * Checks that this object and `other` have the same key. Abstract domain
   * operations here only operate on `CalleePortFramesV2` that have the same
   * key. The only exception is if one of them `is_bottom()`.
   */
  bool has_same_key(const CalleePortFramesV2& other) const {
    return callee_port_ == other.callee_port() &&
        is_result_or_receiver_sinks_ == other.is_result_or_receiver_sinks_;
  }

 private:
  // Note that for result and receiver sinks, this Access Path will only contain
  // a root
  AccessPath callee_port_;
  FramesByKind frames_;
  bool is_result_or_receiver_sinks_;
  // output_paths_ are used only for result or receiver sinks (should be bottom
  // for all other frames). These keep track of the paths within the
  // result/receiver sink that have been read from this taint. The paths are
  // then used to infer sinks and propagations.
  PathTreeDomain output_paths_;

  // TODO(T91357916): Move local_features here from `Frame`.
  LocalPositionSet local_positions_;
};

} // namespace marianatrench
