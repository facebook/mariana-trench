/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/CalleeFrames.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

CalleeFrames::CalleeFrames(std::initializer_list<Frame> frames)
    : callee_(nullptr) {
  for (const auto& frame : frames) {
    add(frame);
  }
}

void CalleeFrames::add(const Frame& frame) {
  if (callee_ == nullptr) {
    callee_ = frame.callee();
  } else {
    mt_assert(callee_ == frame.callee());
  }

  // TODO (T91357916): GroupHashedSetAbstractDomain could be more efficient.
  // It supports an `add` operation that avoids making a copy.
  frames_.update(
      frame.call_position(), [&](const CallPositionFrames& old_frames) {
        auto new_frames = old_frames;
        new_frames.add(frame);
        return new_frames;
      });
}

bool CalleeFrames::leq(const CalleeFrames& other) const {
  mt_assert(is_bottom() || other.is_bottom() || callee_ == other.callee());
  return frames_.leq(other.frames_);
}

bool CalleeFrames::equals(const CalleeFrames& other) const {
  mt_assert(is_bottom() || other.is_bottom() || callee_ == other.callee());
  return frames_.equals(other.frames_);
}

void CalleeFrames::join_with(const CalleeFrames& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    mt_assert(callee_ == nullptr);
    callee_ = other.callee();
  }
  mt_assert(other.is_bottom() || callee_ == other.callee());

  frames_.join_with(other.frames_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void CalleeFrames::widen_with(const CalleeFrames& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    mt_assert(callee_ == nullptr);
    callee_ = other.callee();
  }
  mt_assert(other.is_bottom() || callee_ == other.callee());

  frames_.widen_with(other.frames_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void CalleeFrames::meet_with(const CalleeFrames& other) {
  if (is_bottom()) {
    mt_assert(callee_ == nullptr);
    callee_ = other.callee();
  }
  mt_assert(other.is_bottom() || callee_ == other.callee());

  frames_.meet_with(other.frames_);
}

void CalleeFrames::narrow_with(const CalleeFrames& other) {
  if (is_bottom()) {
    mt_assert(callee_ == nullptr);
    callee_ = other.callee();
  }
  mt_assert(other.is_bottom() || callee_ == other.callee());

  frames_.narrow_with(other.frames_);
}

void CalleeFrames::difference_with(const CalleeFrames& other) {
  if (is_bottom()) {
    mt_assert(callee_ == nullptr);
    callee_ = other.callee();
  }
  mt_assert(other.is_bottom() || callee_ == other.callee());

  frames_.difference_like_operation(
      other.frames_,
      [](const CallPositionFrames& frames_left,
         const CallPositionFrames& frames_right) {
        auto frames_copy = frames_left;
        frames_copy.difference_with(frames_right);
        return frames_copy;
      });
}

void CalleeFrames::map(const std::function<void(Frame&)>& f) {
  frames_.map([&](const CallPositionFrames& frames) {
    auto new_frames = frames;
    new_frames.map(f);
    return new_frames;
  });
}

void CalleeFrames::add_inferred_features(const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map([&features](Frame& frame) { frame.add_inferred_features(features); });
}

LocalPositionSet CalleeFrames::local_positions() const {
  auto result = LocalPositionSet::bottom();
  for (const auto& [_, frames] : frames_.bindings()) {
    result.join_with(frames.local_positions());
  }
  return result;
}

void CalleeFrames::add_local_position(const Position* position) {
  map([position](Frame& frame) { frame.add_local_position(position); });
}

void CalleeFrames::set_local_positions(const LocalPositionSet& positions) {
  map([&positions](Frame& frame) { frame.set_local_positions(positions); });
}

void CalleeFrames::add_inferred_features_and_local_position(
    const FeatureMayAlwaysSet& features,
    const Position* MT_NULLABLE position) {
  if (features.empty() && position == nullptr) {
    return;
  }

  map([&features, position](Frame& frame) {
    if (!features.empty()) {
      frame.add_inferred_features(features);
    }
    if (position != nullptr) {
      frame.add_local_position(position);
    }
  });
}

std::ostream& operator<<(std::ostream& out, const CalleeFrames& frames) {
  if (frames.is_top()) {
    return out << "T";
  } else {
    out << "[";
    for (const auto& [position, frames] : frames.frames_.bindings()) {
      out << "FramesByPosition(position=" << show(position) << ","
          << "frames=" << frames << "),";
    }
    return out << "]";
  }
}

} // namespace marianatrench
