/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/TaintV1.h>

namespace marianatrench {

TaintV1::TaintV1(std::initializer_list<Frame> frames) {
  for (const auto& frame : frames) {
    add(frame);
  }
}

TaintV1::TaintV1(std::initializer_list<FrameSet> frames_set) {
  for (const auto& frames : frames_set) {
    add(frames);
  }
}

TaintV1FramesIterator TaintV1::frames_iterator() const {
  return TaintV1FramesIterator(*this);
}

void TaintV1::add(const Frame& frame) {
  set_.add(FrameSet{frame});
}

void TaintV1::add(const FrameSet& frames) {
  set_.add(frames);
}

bool TaintV1::leq(const TaintV1& other) const {
  return set_.leq(other.set_);
}

bool TaintV1::equals(const TaintV1& other) const {
  return set_.equals(other.set_);
}

void TaintV1::join_with(const TaintV1& other) {
  set_.join_with(other.set_);
}

void TaintV1::widen_with(const TaintV1& other) {
  set_.widen_with(other.set_);
}

void TaintV1::meet_with(const TaintV1& other) {
  set_.meet_with(other.set_);
}

void TaintV1::narrow_with(const TaintV1& other) {
  set_.narrow_with(other.set_);
}

void TaintV1::difference_with(const TaintV1& other) {
  set_.difference_with(other.set_);
}

void TaintV1::add_inferred_features(const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map([&features](FrameSet& frames) {
    frames.add_inferred_features(features);
  });
}

void TaintV1::add_local_position(const Position* position) {
  map([position](FrameSet& frames) { frames.add_local_position(position); });
}

void TaintV1::set_local_positions(const LocalPositionSet& positions) {
  map([&positions](FrameSet& frames) {
    frames.set_local_positions(positions);
  });
}

LocalPositionSet TaintV1::local_positions() const {
  auto result = LocalPositionSet::bottom();
  for (const auto& frame_set : set_) {
    result.join_with(frame_set.local_positions());
  }
  return result;
}

void TaintV1::add_inferred_features_and_local_position(
    const FeatureMayAlwaysSet& features,
    const Position* MT_NULLABLE position) {
  if (features.empty() && position == nullptr) {
    return;
  }

  map([&features, position](FrameSet& frames) {
    frames.add_inferred_features_and_local_position(features, position);
  });
}

TaintV1 TaintV1::propagate(
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
  TaintV1 result;
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

TaintV1 TaintV1::attach_position(const Position* position) const {
  TaintV1 result;
  for (const auto& frames : set_) {
    result.add(frames.attach_position(position));
  }
  return result;
}

TaintV1 TaintV1::transform_kind_with_features(
    const std::function<std::vector<const Kind*>(const Kind*)>& transform_kind,
    const std::function<FeatureMayAlwaysSet(const Kind*)>& add_features) const {
  TaintV1 new_taint;
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
        new_frame_set.add_inferred_features(add_features(new_kind));
        new_taint.add(new_frame_set);
      }
    }
  }
  return new_taint;
}

TaintV1 TaintV1::from_json(const Json::Value& value, Context& context) {
  TaintV1 taint;
  for (const auto& frame_value : JsonValidation::null_or_array(value)) {
    taint.add(Frame::from_json(frame_value, context));
  }
  return taint;
}

Json::Value TaintV1::to_json() const {
  auto taint = Json::Value(Json::arrayValue);
  for (const auto& frames : set_) {
    for (const auto& frame : frames) {
      taint.append(frame.to_json());
    }
  }
  return taint;
}

std::ostream& operator<<(std::ostream& out, const TaintV1& taint) {
  return out << taint.set_;
}

void TaintV1::append_callee_port(
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

void TaintV1::update_non_leaf_positions(
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

void TaintV1::filter_invalid_frames(
    const std::function<bool(const Method*, const AccessPath&, const Kind*)>&
        is_valid) {
  map([&](FrameSet& frames) {
    frames.filter([&](const Frame& frame) {
      return is_valid(frame.callee(), frame.callee_port(), frame.kind());
    });
  });
}

bool TaintV1::contains_kind(const Kind* kind) const {
  return std::any_of(set_.begin(), set_.end(), [&](const FrameSet& frames) {
    return frames.kind() == kind;
  });
}

std::unordered_map<const Kind*, TaintV1> TaintV1::partition_by_kind() const {
  // This could also call the generic partition_by_kind<T>(map_kind).
  // Sticking with a custom implementation because this is very slightly more
  // optimal (does not need to check if kind already exists in the result),
  // gets called rather frequently, and is quite simple.
  std::unordered_map<const Kind*, TaintV1> result;
  for (const auto& frame_set : set_) {
    result.emplace(frame_set.kind(), TaintV1{frame_set});
  }
  return result;
}

FeatureMayAlwaysSet TaintV1::features_joined() const {
  auto features = FeatureMayAlwaysSet::bottom();
  for (const auto& frame : frames_iterator()) {
    features.join_with(frame.features());
  }
  return features;
}

void TaintV1::map(const std::function<void(FrameSet&)>& f) {
  set_.map(f);
}

void TaintV1::filter(const std::function<bool(const FrameSet&)>& predicate) {
  set_.filter(predicate);
}

} // namespace marianatrench
