/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Taint.h>

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
    const FeatureMayAlwaysSet& extra_features,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments)
    const {
  Taint result;
  for (const auto& frames : set_) {
    auto propagated = frames.propagate(
        caller,
        callee,
        callee_port,
        call_position,
        maximum_source_sink_distance,
        context,
        source_register_types,
        source_constant_arguments);
    if (propagated.is_bottom()) {
      continue;
    }
    propagated.add_inferred_features(extra_features);
    result.add(propagated);
  }
  return result;
}

Taint Taint::attach_position(const Position* position) const {
  Taint result;
  for (const auto& frames : set_) {
    result.add(frames.attach_position(position));
  }
  return result;
}

Taint Taint::transform_map_kind(
    const std::function<std::vector<const Kind*>(const Kind*)>& transform_kind,
    const std::function<void(FrameSet&)>& map_frame_set) const {
  Taint new_taint;
  for (const auto& frame_set : set_) {
    const auto* old_kind = frame_set.kind();
    auto new_kinds = transform_kind(old_kind);
    if (new_kinds.empty()) {
      continue;
    } else if (new_kinds.size() == 1 && new_kinds.front() == old_kind) {
      new_taint.add(frame_set); // no transformation
    } else {
      for (const auto* new_kind : new_kinds) {
        // Even if new_kind == old_kind for some new_kind, perform map_frame_set
        // because a transformation occurred.
        auto new_frame_set = frame_set.with_kind(new_kind);
        map_frame_set(new_frame_set);
        new_taint.add(new_frame_set);
      }
    }
  }
  return new_taint;
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

void Taint::append_callee_port(
    Path::Element path_element,
    const std::function<bool(const Kind*)>& filter) {
  map([&](FrameSet& frames) {
    if (filter(frames.kind())) {
      frames.map([path_element](Frame& frame) {
        frame.callee_port_append(path_element);
      });
    }
  });
}

void Taint::update_non_leaf_positions(
    const std::function<
        const Position*(const Method*, const AccessPath&, const Position*)>&
        new_call_position,
    const std::function<LocalPositionSet(const LocalPositionSet&)>&
        new_local_positions) {
  map([&](FrameSet& frames) {
    auto new_frames = FrameSet::bottom();
    for (const auto& frame : frames) {
      if (frame.is_leaf()) {
        new_frames.add(frame);
      } else {
        auto new_frame = Frame(
            frame.kind(),
            frame.callee_port(),
            frame.callee(),
            frame.field_callee(),
            new_call_position(
                frame.callee(), frame.callee_port(), frame.call_position()),
            frame.distance(),
            frame.origins(),
            frame.field_origins(),
            frame.inferred_features(),
            frame.locally_inferred_features(),
            frame.user_features(),
            frame.via_type_of_ports(),
            frame.via_value_of_ports(),
            new_local_positions(frame.local_positions()),
            frame.canonical_names());
        new_frames.add(new_frame);
      }
    }
    frames = std::move(new_frames);
  });
}

void Taint::filter_invalid_frames(
    const std::function<bool(const Method*, const AccessPath&, const Kind*)>&
        is_valid) {
  map([&](FrameSet& frames) {
    frames.filter([&](const Frame& frame) {
      return is_valid(frame.callee(), frame.callee_port(), frame.kind());
    });
  });
}

bool Taint::contains_kind(const Kind* kind) const {
  return std::any_of(begin(), end(), [&](const FrameSet& frames) {
    return frames.kind() == kind;
  });
}

} // namespace marianatrench
