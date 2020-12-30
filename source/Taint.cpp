/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Taint.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

Taint::Taint(std::initializer_list<Frame> frames) {
  for (const auto& frame : frames) {
    add(frame);
  }
}

Taint::Taint(std::initializer_list<FrameSet> frames_set) {
  for (const auto& frames : frames_set) {
    add(frames);
  }
}

void Taint::add(const Frame& frame) {
  set_.add(FrameSet{frame});
}

void Taint::add(const FrameSet& frames) {
  set_.add(frames);
}

bool Taint::leq(const Taint& other) const {
  return set_.leq(other.set_);
}

bool Taint::equals(const Taint& other) const {
  return set_.equals(other.set_);
}

void Taint::join_with(const Taint& other) {
  set_.join_with(other.set_);
}

void Taint::widen_with(const Taint& other) {
  set_.widen_with(other.set_);
}

void Taint::meet_with(const Taint& other) {
  set_.meet_with(other.set_);
}

void Taint::narrow_with(const Taint& other) {
  set_.narrow_with(other.set_);
}

void Taint::difference_with(const Taint& other) {
  set_.difference_with(other.set_);
}

void Taint::map(const std::function<void(FrameSet&)>& f) {
  set_.map(f);
}

void Taint::filter(const std::function<bool(const FrameSet&)>& predicate) {
  set_.filter(predicate);
}

void Taint::add_inferred_features(const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map([&features](FrameSet& frames) {
    frames.add_inferred_features(features);
  });
}

void Taint::add_local_position(const Position* position) {
  map([position](FrameSet& frames) { frames.add_local_position(position); });
}

void Taint::set_local_positions(const LocalPositionSet& positions) {
  map([&positions](FrameSet& frames) {
    frames.set_local_positions(positions);
  });
}

void Taint::add_inferred_features_and_local_position(
    const FeatureMayAlwaysSet& features,
    const Position* MT_NULLABLE position) {
  if (features.empty() && position == nullptr) {
    return;
  }

  map([&features, position](FrameSet& frames) {
    frames.add_inferred_features_and_local_position(features, position);
  });
}

Taint Taint::propagate(
    const Method* caller,
    const Method* callee,
    const AccessPath& callee_port,
    const Position* call_position,
    int maximum_source_sink_distance,
    const FeatureMayAlwaysSet& extra_features) const {
  Taint result;
  for (const auto& frames : set_) {
    auto propagated = frames.propagate(
        caller,
        callee,
        callee_port,
        call_position,
        maximum_source_sink_distance);
    if (propagated.is_bottom()) {
      continue;
    }
    propagated.add_inferred_features(extra_features);
    result.add(propagated);
  }
  return result;
}

Taint Taint::attach_position(
    const Position* position,
    const FeatureMayAlwaysSet& extra_features) const {
  Taint result;
  for (const auto& frames : set_) {
    result.add(frames.attach_position(position, extra_features));
  }
  return result;
}

Taint Taint::from_json(const Json::Value& value, Context& context) {
  Taint taint;
  for (const auto& frame_value : JsonValidation::null_or_array(value)) {
    taint.add(Frame::from_json(frame_value, context));
  }
  return taint;
}

Json::Value Taint::to_json() const {
  auto taint = Json::Value(Json::arrayValue);
  for (const auto& frames : set_) {
    for (const auto& frame : frames) {
      taint.append(frame.to_json());
    }
  }
  return taint;
}

std::ostream& operator<<(std::ostream& out, const Taint& taint) {
  return out << taint.set_;
}

} // namespace marianatrench
