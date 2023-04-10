/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/CallPositionFrames.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

CallPositionFrames::CallPositionFrames(
    std::initializer_list<TaintConfig> configs)
    : position_(nullptr) {
  for (const auto& config : configs) {
    add(config);
  }
}

void CallPositionFrames::add(const TaintConfig& config) {
  if (position_ == nullptr) {
    position_ = config.call_position();
  } else {
    mt_assert(position_ == config.call_position());
  }

  frames_.add(CalleePortFrames{config});
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

void CallPositionFrames::set_origins_if_empty(const MethodSet& origins) {
  frames_.map([&origins](CalleePortFrames callee_port_frames) {
    callee_port_frames.set_origins_if_empty(origins);
    return callee_port_frames;
  });
}

void CallPositionFrames::set_field_origins_if_empty_with_field_callee(
    const Field* field) {
  frames_.map([field](CalleePortFrames callee_port_frames) {
    callee_port_frames.set_field_origins_if_empty_with_field_callee(field);
    return callee_port_frames;
  });
}

FeatureMayAlwaysSet CallPositionFrames::inferred_features() const {
  auto result = FeatureMayAlwaysSet::bottom();
  for (const auto& callee_port_frames : frames_) {
    result.join_with(callee_port_frames.inferred_features());
  }
  return result;
}

void CallPositionFrames::add_locally_inferred_features(
    const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map([&features](Frame frame) {
    frame.add_locally_inferred_features(features);
    return frame;
  });
}

LocalPositionSet CallPositionFrames::local_positions() const {
  auto result = LocalPositionSet::bottom();
  for (const auto& callee_port_frames : frames_) {
    result.join_with(callee_port_frames.local_positions());
  }
  return result;
}

void CallPositionFrames::add_local_position(const Position* position) {
  frames_.map([position](CalleePortFrames callee_port_frames) {
    callee_port_frames.add_local_position(position);
    return callee_port_frames;
  });
}

void CallPositionFrames::set_local_positions(
    const LocalPositionSet& positions) {
  frames_.map([positions](CalleePortFrames callee_port_frames) {
    callee_port_frames.set_local_positions(positions);
    return callee_port_frames;
  });
}

void CallPositionFrames::add_locally_inferred_features_and_local_position(
    const FeatureMayAlwaysSet& features,
    const Position* MT_NULLABLE position) {
  if (features.empty() && position == nullptr) {
    return;
  }

  map([&features](Frame frame) {
    if (!features.empty()) {
      frame.add_locally_inferred_features(features);
    }
    return frame;
  });

  if (position != nullptr) {
    add_local_position(position);
  }
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

      result.add(CalleePortFrames{TaintConfig(
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
          frame.canonical_names(),
          /* output_paths */ {},
          callee_port_frames.local_positions(),
          CallInfo::Origin)});
    }
  }

  return CallPositionFrames(position, result);
}

CallPositionFrames CallPositionFrames::apply_transform(
    const KindFactory& kind_factory,
    const TransformsFactory& transforms_factory,
    const UsedKinds& used_kinds,
    const TransformList* local_transforms) const {
  FramesByCalleePort frames_by_callee_port;

  for (const auto& callee_port_frames : frames_) {
    frames_by_callee_port.add(callee_port_frames.apply_transform(
        kind_factory, transforms_factory, used_kinds, local_transforms));
  }

  return CallPositionFrames{position_, frames_by_callee_port};
}

std::unordered_map<const Position*, CallPositionFrames>
CallPositionFrames::map_positions(
    const std::function<const Position*(const AccessPath&, const Position*)>&
        new_call_position,
    const std::function<LocalPositionSet(const LocalPositionSet&)>&
        new_local_positions) const {
  std::unordered_map<const Position*, CallPositionFrames> result;
  for (const auto& callee_port_frames : frames_) {
    auto call_position =
        new_call_position(callee_port_frames.callee_port(), position_);
    auto local_positions =
        new_local_positions(callee_port_frames.local_positions());

    auto new_frames = CalleePortFrames::bottom();
    for (const auto& frame : callee_port_frames) {
      // TODO(T91357916): Move call_position out of Frame and store it only in
      // `CallPositionFrames` so we do not need to update every frame.
      auto new_frame = TaintConfig(
          frame.kind(),
          frame.callee_port(),
          frame.callee(),
          frame.field_callee(),
          call_position,
          frame.distance(),
          frame.origins(),
          frame.field_origins(),
          frame.inferred_features(),
          frame.locally_inferred_features(),
          frame.user_features(),
          frame.via_type_of_ports(),
          frame.via_value_of_ports(),
          frame.canonical_names(),
          /* output_paths */ {},
          /* local_positions */ {},
          /* call_info */ frame.call_info());
      new_frames.add(new_frame);
    }

    if (!new_frames.is_bottom()) {
      new_frames.set_local_positions(local_positions);
    }

    auto found = result.find(call_position);
    auto frames =
        CallPositionFrames(call_position, FramesByCalleePort(new_frames));
    if (found != result.end()) {
      found->second.join_with(frames);
    } else {
      result.emplace(call_position, frames);
    }
  }
  return result;
}

void CallPositionFrames::filter_invalid_frames(
    const std::function<bool(const Method*, const AccessPath&, const Kind*)>&
        is_valid) {
  frames_.map([&is_valid](CalleePortFrames callee_port_frames) {
    callee_port_frames.filter_invalid_frames(is_valid);
    return callee_port_frames;
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

Json::Value CallPositionFrames::to_json(
    const Method* MT_NULLABLE callee,
    CallInfo call_info) const {
  auto taint = Json::Value(Json::arrayValue);
  for (const auto& callee_port_frames : frames_) {
    auto frames_json = callee_port_frames.to_json(callee, position_, call_info);
    taint.append(frames_json);
  }
  return taint;
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
