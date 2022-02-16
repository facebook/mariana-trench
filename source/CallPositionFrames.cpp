/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/CallPositionFrames.h>
#include <mariana-trench/Features.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

CallPositionFrames::CallPositionFrames(std::initializer_list<Frame> frames)
    : position_(nullptr) {
  for (const auto& frame : frames) {
    add(frame);
  }
}

void CallPositionFrames::add(const Frame& frame) {
  if (position_ == nullptr) {
    position_ = frame.call_position();
  } else {
    mt_assert(position_ == frame.call_position());
  }

  frames_.update(frame.kind(), [&](const Frames& old_frames) {
    auto new_frames = old_frames;
    new_frames.add(frame);
    return new_frames;
  });
}

bool CallPositionFrames::leq(const CallPositionFrames& other) const {
  mt_assert(position_ == other.position());
  return frames_.leq(other.frames_);
}

bool CallPositionFrames::equals(const CallPositionFrames& other) const {
  mt_assert(position_ == other.position());
  return frames_.equals(other.frames_);
}

void CallPositionFrames::join_with(const CallPositionFrames& other) {
  mt_if_expensive_assert(auto previous = *this);
  mt_assert(position_ == other.position());

  frames_.join_with(other.frames_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void CallPositionFrames::widen_with(const CallPositionFrames& other) {
  mt_if_expensive_assert(auto previous = *this);
  mt_assert(position_ == other.position());

  frames_.widen_with(other.frames_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void CallPositionFrames::meet_with(const CallPositionFrames& other) {
  mt_assert(position_ == other.position());
  frames_.meet_with(other.frames_);
}

void CallPositionFrames::narrow_with(const CallPositionFrames& other) {
  mt_assert(position_ == other.position());
  frames_.narrow_with(other.frames_);
}

void CallPositionFrames::difference_with(const CallPositionFrames& other) {
  mt_assert(other.position_ == nullptr || position_ == other.position());

  frames_.difference_like_operation(
      other.frames_, [](const Frames& frames_left, const Frames& frames_right) {
        auto frames_copy = frames_left;
        frames_copy.difference_with(frames_right);
        return frames_copy;
      });
}

void CallPositionFrames::map(const std::function<void(Frame&)>& f) {
  frames_.map([&](const Frames& frames) {
    auto new_frames = frames;
    new_frames.map(f);
    return new_frames;
  });
}

void CallPositionFrames::add_inferred_features(
    const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map([&features](Frame& frame) { frame.add_inferred_features(features); });
}

LocalPositionSet CallPositionFrames::local_positions() const {
  // Ideally this can be stored within `CallPositionFrames` instead of `Frame`.
  // Local positions should be the same for a given (callee, call_position).
  auto result = LocalPositionSet::bottom();
  for (const auto& [_, frames] : frames_.bindings()) {
    for (const auto& frame : frames) {
      result.join_with(frame.local_positions());
    }
  }
  return result;
}

void CallPositionFrames::add_local_position(const Position* position) {
  map([position](Frame& frame) { frame.add_local_position(position); });
}

void CallPositionFrames::set_local_positions(
    const LocalPositionSet& positions) {
  map([&positions](Frame& frame) { frame.set_local_positions(positions); });
}

void CallPositionFrames::add_inferred_features_and_local_position(
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

} // namespace marianatrench
