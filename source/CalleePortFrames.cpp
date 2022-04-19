/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/CalleePortFrames.h>
#include <mariana-trench/Features.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

CalleePortFrames::CalleePortFrames(std::initializer_list<Frame> frames)
    : callee_port_(Root(Root::Kind::Leaf)) {
  for (const auto& frame : frames) {
    add(frame);
  }
}

void CalleePortFrames::add(const Frame& frame) {
  if (is_bottom()) {
    callee_port_ = frame.callee_port();
  } else {
    mt_assert(callee_port_ == frame.callee_port());
  }

  frames_.update(frame.kind(), [&](const Frames& old_frames) {
    auto new_frames = old_frames;
    new_frames.add(frame);
    return new_frames;
  });
}

bool CalleePortFrames::leq(const CalleePortFrames& other) const {
  mt_assert(
      is_bottom() || other.is_bottom() || callee_port_ == other.callee_port());
  return frames_.leq(other.frames_);
}

bool CalleePortFrames::equals(const CalleePortFrames& other) const {
  mt_assert(
      is_bottom() || other.is_bottom() || callee_port_ == other.callee_port());
  return frames_.equals(other.frames_);
}

void CalleePortFrames::join_with(const CalleePortFrames& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    callee_port_ = other.callee_port();
  }
  mt_assert(other.is_bottom() || callee_port_ == other.callee_port());

  frames_.join_with(other.frames_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void CalleePortFrames::widen_with(const CalleePortFrames& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    callee_port_ = other.callee_port();
  }
  mt_assert(other.is_bottom() || callee_port_ == other.callee_port());

  frames_.widen_with(other.frames_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void CalleePortFrames::meet_with(const CalleePortFrames& other) {
  if (is_bottom()) {
    callee_port_ = other.callee_port();
  }
  mt_assert(other.is_bottom() || callee_port_ == other.callee_port());

  frames_.meet_with(other.frames_);
}

void CalleePortFrames::narrow_with(const CalleePortFrames& other) {
  if (is_bottom()) {
    callee_port_ = other.callee_port();
  }
  mt_assert(other.is_bottom() || callee_port_ == other.callee_port());

  frames_.narrow_with(other.frames_);
}

void CalleePortFrames::difference_with(const CalleePortFrames& other) {
  if (is_bottom()) {
    callee_port_ = other.callee_port();
  }
  mt_assert(other.is_bottom() || callee_port_ == other.callee_port());

  frames_.difference_like_operation(
      other.frames_, [](const Frames& frames_left, const Frames& frames_right) {
        auto frames_copy = frames_left;
        frames_copy.difference_with(frames_right);
        return frames_copy;
      });
}

void CalleePortFrames::map(const std::function<void(Frame&)>& f) {
  frames_.map([&](const Frames& frames) {
    auto new_frames = frames;
    new_frames.map(f);
    return new_frames;
  });
}

void CalleePortFrames::add_inferred_features(
    const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map([&features](Frame& frame) { frame.add_inferred_features(features); });
}

LocalPositionSet CalleePortFrames::local_positions() const {
  // Ideally this can be stored within `CalleePortFrames` instead of `Frame`.
  // Local positions should be the same for a given (callee, call_position).
  auto result = LocalPositionSet::bottom();
  for (const auto& [_, frames] : frames_.bindings()) {
    for (const auto& frame : frames) {
      result.join_with(frame.local_positions());
    }
  }
  return result;
}

void CalleePortFrames::add_local_position(const Position* position) {
  map([position](Frame& frame) { frame.add_local_position(position); });
}

void CalleePortFrames::set_local_positions(const LocalPositionSet& positions) {
  map([&positions](Frame& frame) { frame.set_local_positions(positions); });
}

void CalleePortFrames::add_inferred_features_and_local_position(
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

CalleePortFrames CalleePortFrames::transform_kind_with_features(
    const std::function<std::vector<const Kind*>(const Kind*)>& transform_kind,
    const std::function<FeatureMayAlwaysSet(const Kind*)>& add_features) const {
  FramesByKind new_frames_by_kind;
  for (const auto& [old_kind, frames] : frames_.bindings()) {
    auto new_kinds = transform_kind(old_kind);
    if (new_kinds.empty()) {
      continue;
    } else if (new_kinds.size() == 1 && new_kinds.front() == old_kind) {
      new_frames_by_kind.set(old_kind, frames); // no transformation
    } else {
      for (const auto* new_kind : new_kinds) {
        // Even if new_kind == old_kind for some new_kind, perform map_frame_set
        // because a transformation occurred.
        Frames new_frames;
        auto features_to_add = add_features(new_kind);
        for (const auto& frame : frames) {
          auto new_frame = frame.with_kind(new_kind);
          new_frame.add_inferred_features(features_to_add);
          new_frames.add(new_frame);
        }
        new_frames_by_kind.update(new_kind, [&](const Frames& existing) {
          return existing.join(new_frames);
        });
      }
    }
  }
  return CalleePortFrames(callee_port_, new_frames_by_kind);
}

bool CalleePortFrames::contains_kind(const Kind* kind) const {
  for (const auto& [actual_kind, _] : frames_.bindings()) {
    if (actual_kind == kind) {
      return true;
    }
  }
  return false;
}

std::ostream& operator<<(std::ostream& out, const CalleePortFrames& frames) {
  if (frames.is_top()) {
    return out << "T";
  } else {
    out << "[";
    for (const auto& [kind, frames] : frames.frames_.bindings()) {
      out << "FrameByKind(kind=" << show(kind) << ", frames=" << frames << "),";
    }
    return out << "]";
  }
}

} // namespace marianatrench
