/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

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

  frames_.add(CalleePortFrames({frame}));
}

bool CallPositionFrames::leq(const CallPositionFrames& other) const {
  mt_assert(is_bottom() || other.is_bottom() || position_ == other.position());
  return frames_.leq(other.frames_);
}

bool CallPositionFrames::equals(const CallPositionFrames& other) const {
  mt_assert(is_bottom() || other.is_bottom() || position_ == other.position());
  return frames_.equals(other.frames_);
}

void CallPositionFrames::join_with(const CallPositionFrames& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    mt_assert(position_ == nullptr);
    position_ = other.position();
  }
  mt_assert(other.is_bottom() || position_ == other.position());

  frames_.join_with(other.frames_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void CallPositionFrames::widen_with(const CallPositionFrames& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    mt_assert(position_ == nullptr);
    position_ = other.position();
  }
  mt_assert(other.is_bottom() || position_ == other.position());

  frames_.widen_with(other.frames_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void CallPositionFrames::meet_with(const CallPositionFrames& other) {
  if (is_bottom()) {
    mt_assert(position_ == nullptr);
    position_ = other.position();
  }
  mt_assert(other.is_bottom() || position_ == other.position());

  frames_.meet_with(other.frames_);
}

void CallPositionFrames::narrow_with(const CallPositionFrames& other) {
  if (is_bottom()) {
    mt_assert(position_ == nullptr);
    position_ = other.position();
  }
  mt_assert(other.is_bottom() || position_ == other.position());

  frames_.narrow_with(other.frames_);
}

void CallPositionFrames::difference_with(const CallPositionFrames& other) {
  if (is_bottom()) {
    mt_assert(position_ == nullptr);
    position_ = other.position();
  }
  mt_assert(other.is_bottom() || position_ == other.position());

  frames_.difference_with(other.frames_);
}

void CallPositionFrames::map(const std::function<void(Frame&)>& f) {
  frames_.map(
      [&](CalleePortFrames& callee_port_frames) { callee_port_frames.map(f); });
}

void CallPositionFrames::add_inferred_features(
    const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map([&features](Frame& frame) { frame.add_inferred_features(features); });
}

LocalPositionSet CallPositionFrames::local_positions() const {
  auto result = LocalPositionSet::bottom();
  for (const auto& callee_port_frames : frames_) {
    result.join_with(callee_port_frames.local_positions());
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

CallPositionFrames CallPositionFrames::propagate(
    const Method* callee,
    const AccessPath& callee_port,
    const Position* call_position,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments)
    const {
  if (is_bottom()) {
    return CallPositionFrames::bottom();
  }

  FramesByCalleePort result;
  for (const auto& callee_port_frames : frames_) {
    result.add(callee_port_frames.propagate(
        callee,
        callee_port,
        call_position,
        maximum_source_sink_distance,
        context,
        source_register_types,
        source_constant_arguments));
  }

  if (result.is_bottom()) {
    return CallPositionFrames::bottom();
  }

  return CallPositionFrames(call_position, result);
}

CallPositionFrames CallPositionFrames::attach_position(
    const Position* position) const {
  FramesByCalleePort result;

  // NOTE: This method does more than update the position in frames. It
  // functions similarly to `propagate`. Frame features are propagated here.
  for (const auto& callee_port_frames : frames_) {
    for (const auto& frame : callee_port_frames) {
      if (!frame.is_leaf()) {
        continue;
      }

      result.add(CalleePortFrames({Frame(
          frame.kind(),
          frame.callee_port(),
          /* callee */ nullptr,
          /* field_callee */ nullptr,
          /* call_position */ position,
          /* distance */ 0,
          frame.origins(),
          frame.field_origins(),
          frame.features(),
          // Since CallPositionFrames::attach_position is used (only) for
          // parameter_sinks and return sources which may be included in an
          // issue as a leaf, we need to make sure that those leaf frames in
          // issues contain the user_features as being locally inferred.
          /* locally_inferred_features */
          frame.user_features().is_bottom()
              ? FeatureMayAlwaysSet::bottom()
              : FeatureMayAlwaysSet::make_always(frame.user_features()),
          /* user_features */ FeatureSet::bottom(),
          /* via_type_of_ports */ {},
          /* via_value_of_ports */ {},
          frame.local_positions(),
          frame.canonical_names())}));
    }
  }

  return CallPositionFrames(position, result);
}

CallPositionFrames CallPositionFrames::transform_kind_with_features(
    const std::function<std::vector<const Kind*>(const Kind*)>& transform_kind,
    const std::function<FeatureMayAlwaysSet(const Kind*)>& add_features) const {
  FramesByCalleePort new_frames;
  for (const auto& callee_port_frames : frames_) {
    new_frames.add(callee_port_frames.transform_kind_with_features(
        transform_kind, add_features));
  }

  return CallPositionFrames(position_, new_frames);
}

void CallPositionFrames::append_callee_port(
    Path::Element path_element,
    const std::function<bool(const Kind*)>& /* unused */) {
  // TODO (T91357916): Make CalleePortFrames::append_callee_port work in-place
  // so we can use frames_.map() directly without having to make a copy.
  FramesByCalleePort new_frames;
  for (const auto& callee_port_frames : frames_) {
    // Callee ports can only be updated for artificial sources (see
    // `CalleePortFrames::GroupHash` key). Everything else must be kept as-is.
    if (!callee_port_frames.is_artificial_source_frames()) {
      new_frames.add(callee_port_frames);
    } else {
      new_frames.add(callee_port_frames.append_callee_port(path_element));
    }
  }

  frames_ = new_frames;
}

void CallPositionFrames::filter_invalid_frames(
    const std::function<bool(const Method*, const AccessPath&, const Kind*)>&
        is_valid) {
  frames_.map([&](CalleePortFrames& callee_port_frames) {
    callee_port_frames.filter_invalid_frames(is_valid);
  });
}

bool CallPositionFrames::contains_kind(const Kind* kind) const {
  for (const auto& callee_port_frames : frames_) {
    if (callee_port_frames.contains_kind(kind)) {
      return true;
    }
  }
  return false;
}

std::ostream& operator<<(std::ostream& out, const CallPositionFrames& frames) {
  // No representation for top() because FramesByCalleePort::top() is N/A.
  out << "[";
  for (const auto& callee_port_frames : frames.frames_) {
    out << "FramesByCalleePort(" << show(callee_port_frames) << "),";
  }
  return out << "]";
}

} // namespace marianatrench
