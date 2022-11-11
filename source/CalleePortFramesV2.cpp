/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/CalleePortFramesV2.h>
#include <mariana-trench/Heuristics.h>

namespace marianatrench {

CalleePortFramesV2::CalleePortFramesV2(
    std::initializer_list<TaintConfig> configs)
    : callee_port_(Root(Root::Kind::Leaf)),
      frames_(FramesByKind::bottom()),
      is_result_or_receiver_sinks_(false) {
  for (const auto& config : configs) {
    mt_assert(!config.is_artificial_source());
    add(config);
  }
}

void CalleePortFramesV2::add(const TaintConfig& config) {
  mt_assert(!config.is_artificial_source());
  if (is_bottom()) {
    callee_port_ = config.callee_port();
    is_result_or_receiver_sinks_ = config.is_result_or_receiver_sink();
  } else {
    mt_assert(
        callee_port_ == config.callee_port() &&
        is_result_or_receiver_sinks_ == config.is_result_or_receiver_sink());
  }

  output_paths_.join_with(config.output_paths());
  local_positions_.join_with(config.local_positions());
  frames_.update(config.kind(), [&](const Frames& old_frames) {
    auto new_frames = old_frames;
    new_frames.add(Frame(
        config.kind(),
        config.callee_port(),
        config.callee(),
        config.field_callee(),
        config.call_position(),
        config.distance(),
        config.origins(),
        config.field_origins(),
        config.inferred_features(),
        config.locally_inferred_features(),
        config.user_features(),
        config.via_type_of_ports(),
        config.via_value_of_ports(),
        config.canonical_names()));
    return new_frames;
  });
}

bool CalleePortFramesV2::leq(const CalleePortFramesV2& other) const {
  if (is_bottom()) {
    return true;
  } else if (other.is_bottom()) {
    return false;
  }
  mt_assert(has_same_key(other));
  return frames_.leq(other.frames_) && output_paths_.leq(other.output_paths_) &&
      local_positions_.leq(other.local_positions_);
}

bool CalleePortFramesV2::equals(const CalleePortFramesV2& other) const {
  mt_assert(is_bottom() || other.is_bottom() || has_same_key(other));
  return frames_.equals(other.frames_) &&
      output_paths_.equals(other.output_paths_) &&
      local_positions_.equals(other.local_positions_);
}

void CalleePortFramesV2::join_with(const CalleePortFramesV2& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    callee_port_ = other.callee_port();
    is_result_or_receiver_sinks_ = other.is_result_or_receiver_sinks_;
  }
  mt_assert(other.is_bottom() || has_same_key(other));

  frames_.join_with(other.frames_);
  output_paths_.join_with(other.output_paths_);
  // Approximate the output paths here to avoid storing very large trees during
  // the analysis of a method. These paths will not be read from during the
  // analysis of a method and will be collapsed when the result/receiver sink
  // causes sink/propagation inference. So pre-emptively collapse here for
  // better performance
  output_paths_.collapse_deeper_than(Heuristics::kMaxInputPathDepth);
  output_paths_.limit_leaves(Heuristics::kMaxInputPathLeaves);
  local_positions_.join_with(other.local_positions_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void CalleePortFramesV2::widen_with(const CalleePortFramesV2& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    callee_port_ = other.callee_port();
    is_result_or_receiver_sinks_ = other.is_result_or_receiver_sinks_;
  }
  mt_assert(other.is_bottom() || has_same_key(other));

  frames_.widen_with(other.frames_);
  output_paths_.widen_with(other.output_paths_);
  local_positions_.widen_with(other.local_positions_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void CalleePortFramesV2::meet_with(const CalleePortFramesV2& other) {
  if (is_bottom()) {
    callee_port_ = other.callee_port();
    is_result_or_receiver_sinks_ = other.is_result_or_receiver_sinks_;
  }
  mt_assert(other.is_bottom() || has_same_key(other));

  frames_.meet_with(other.frames_);
  output_paths_.meet_with(other.output_paths_);
  local_positions_.meet_with(other.local_positions_);
}

void CalleePortFramesV2::narrow_with(const CalleePortFramesV2& other) {
  if (is_bottom()) {
    callee_port_ = other.callee_port();
    is_result_or_receiver_sinks_ = other.is_result_or_receiver_sinks_;
  }
  mt_assert(other.is_bottom() || has_same_key(other));

  frames_.narrow_with(other.frames_);
  output_paths_.narrow_with(other.output_paths_);
  local_positions_.narrow_with(other.local_positions_);
}

std::ostream& operator<<(std::ostream& out, const CalleePortFramesV2& frames) {
  if (frames.is_top()) {
    return out << "T";
  } else {
    out << "CalleePortFramesV2("
        << "callee_port=" << frames.callee_port()
        << ", is_result_or_receiver_sinks="
        << frames.is_result_or_receiver_sinks_;

    const auto& local_positions = frames.local_positions();
    if (!local_positions.is_bottom() && !local_positions.empty()) {
      out << ", local_positions=" << frames.local_positions();
    }

    if (!frames.output_paths_.is_bottom()) {
      out << ", output_paths=" << frames.output_paths_;
    }

    out << ", frames=[";
    for (const auto& [kind, frames] : frames.frames_.bindings()) {
      out << "FrameByKind(kind=" << show(kind) << ", frames=" << frames << "),";
    }
    return out << "])";
  }
}

void CalleePortFramesV2::add(const Frame& frame) {
  mt_assert(!frame.is_artificial_source());
  mt_assert(
      !frame.is_result_or_receiver_sink() ||
      frame.callee_port().path().empty());
  if (is_bottom()) {
    callee_port_ = frame.callee_port();
    is_result_or_receiver_sinks_ = frame.is_result_or_receiver_sink();
  } else {
    mt_assert(
        callee_port_ == frame.callee_port() &&
        is_result_or_receiver_sinks_ == frame.is_result_or_receiver_sink());
  }

  frames_.update(frame.kind(), [&](const Frames& old_frames) {
    auto new_frames = old_frames;
    new_frames.add(frame);
    return new_frames;
  });
}

} // namespace marianatrench
