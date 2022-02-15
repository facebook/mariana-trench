/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/iterator/transform_iterator.hpp>
#include <initializer_list>
#include <ostream>

#include <json/json.h>

#include <AbstractDomain.h>
#include <PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/FlattenIterator.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/GroupHashedSetAbstractDomain.h>

namespace marianatrench {

/**
 * Represents a set of frames with the same call position.
 * Based on its position in `Taint`, it is expected that all frames within
 * this class have the same callee and call position.
 */
class CallPositionFrames final
    : public sparta::AbstractDomain<CallPositionFrames> {
 private:
  using Frames =
      GroupHashedSetAbstractDomain<Frame, Frame::GroupHash, Frame::GroupEqual>;
  using FramesByKind =
      sparta::PatriciaTreeMapAbstractPartition<const Kind*, Frames>;

 private:
  // Iterator based on `FlattenIterator`.

  struct KindToFramesMapDereference {
    static Frames::iterator begin(const std::pair<const Kind*, Frames>& pair) {
      return pair.second.begin();
    }
    static Frames::iterator end(const std::pair<const Kind*, Frames>& pair) {
      return pair.second.end();
    }
  };

  using ConstIterator = FlattenIterator<
      /* OuterIterator */ FramesByKind::MapType::iterator,
      /* InnerIterator */ Frames::iterator,
      KindToFramesMapDereference>;

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
  explicit CallPositionFrames(
      const Position* MT_NULLABLE position,
      FramesByKind frames)
      : position_(position), frames_(std::move(frames)) {}

 public:
  /* Create the bottom (i.e, empty) frame set. */
  CallPositionFrames() : position_(nullptr), frames_(FramesByKind::bottom()) {}

  explicit CallPositionFrames(std::initializer_list<Frame> frames);

  CallPositionFrames(const CallPositionFrames&) = default;
  CallPositionFrames(CallPositionFrames&&) = default;
  CallPositionFrames& operator=(const CallPositionFrames&) = default;
  CallPositionFrames& operator=(CallPositionFrames&&) = default;

  static CallPositionFrames bottom() {
    return CallPositionFrames(
        /* position */ nullptr, FramesByKind::bottom());
  }

  static CallPositionFrames top() {
    return CallPositionFrames(
        /* position */ nullptr, FramesByKind::top());
  }

  bool is_bottom() const override {
    return frames_.is_bottom();
  }

  bool is_top() const override {
    return frames_.is_top();
  }

  void set_to_bottom() override {
    position_ = nullptr;
    frames_.set_to_bottom();
  }

  void set_to_top() override {
    position_ = nullptr;
    frames_.set_to_top();
  }

  bool empty() const {
    return frames_.is_bottom();
  }

  const Position* MT_NULLABLE position() const {
    return position_;
  }

  void add(const Frame& frame);

  bool leq(const CallPositionFrames& other) const override;

  bool equals(const CallPositionFrames& other) const override;

  void join_with(const CallPositionFrames& other) override;

  void widen_with(const CallPositionFrames& other) override;

  void meet_with(const CallPositionFrames& other) override;

  void narrow_with(const CallPositionFrames& other) override;

  void difference_with(const CallPositionFrames& other);

  ConstIterator begin() const {
    return ConstIterator(frames_.bindings().begin(), frames_.bindings().end());
  }

  ConstIterator end() const {
    return ConstIterator(frames_.bindings().end(), frames_.bindings().end());
  }

 private:
  const Position* MT_NULLABLE position_;
  FramesByKind frames_;

  // TODO(T91357916): Move local_positions and local_features here from `Frame`.
};

} // namespace marianatrench
