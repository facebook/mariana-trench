/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/TaintV2.h>

namespace marianatrench {

TaintV2::TaintV2(std::initializer_list<Frame> frames) {
  for (const auto& frame : frames) {
    add(frame);
  }
}

void TaintV2::add(const Frame& frame) {
  set_.add(CalleeFrames{frame});
}

bool TaintV2::leq(const TaintV2& other) const {
  return set_.leq(other.set_);
}

bool TaintV2::equals(const TaintV2& other) const {
  return set_.equals(other.set_);
}

void TaintV2::join_with(const TaintV2& other) {
  set_.join_with(other.set_);
}

void TaintV2::widen_with(const TaintV2& other) {
  set_.widen_with(other.set_);
}

void TaintV2::meet_with(const TaintV2& other) {
  set_.meet_with(other.set_);
}

void TaintV2::narrow_with(const TaintV2& other) {
  set_.narrow_with(other.set_);
}

void TaintV2::difference_with(const TaintV2& other) {
  set_.difference_with(other.set_);
}

void TaintV2::add_inferred_features(const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map([&features](CalleeFrames& frames) {
    frames.add_inferred_features(features);
  });
}

void TaintV2::add_local_position(const Position* position) {
  map([position](CalleeFrames& frames) {
    frames.add_local_position(position);
  });
}

void TaintV2::set_local_positions(const LocalPositionSet& positions) {
  map([&positions](CalleeFrames& frames) {
    frames.set_local_positions(positions);
  });
}

void TaintV2::add_inferred_features_and_local_position(
    const FeatureMayAlwaysSet& features,
    const Position* MT_NULLABLE position) {
  if (features.empty() && position == nullptr) {
    return;
  }

  map([&features, position](CalleeFrames& frames) {
    frames.add_inferred_features_and_local_position(features, position);
  });
}

TaintV2 TaintV2::propagate(
    const Method* callee,
    const AccessPath& callee_port,
    const Position* call_position,
    int maximum_source_sink_distance,
    const FeatureMayAlwaysSet& extra_features,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments)
    const {
  TaintV2 result;
  for (const auto& frames : set_) {
    auto propagated = frames.propagate(
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

TaintV2 TaintV2::attach_position(const Position* position) const {
  TaintV2 result;
  for (const auto& frames : set_) {
    result.add(frames.attach_position(position));
  }
  return result;
}

TaintV2 TaintV2::transform_kind_with_features(
    const std::function<std::vector<const Kind*>(const Kind*)>& transform_kind,
    const std::function<FeatureMayAlwaysSet(const Kind*)>& add_features) const {
  TaintV2 new_taint;
  for (const auto& callee_frames : set_) {
    new_taint.add(callee_frames.transform_kind_with_features(
        transform_kind, add_features));
  }
  return new_taint;
}

void TaintV2::append_callee_port(
    Path::Element path_element,
    const std::function<bool(const Kind*)>& filter) {
  map([&](CalleeFrames& frames) {
    frames.append_callee_port(path_element, filter);
  });
}

void TaintV2::update_non_leaf_positions(
    const std::function<
        const Position*(const Method*, const AccessPath&, const Position*)>&
        new_call_position,
    const std::function<LocalPositionSet(const LocalPositionSet&)>&
        new_local_positions) {
  map([&](CalleeFrames& frames) {
    if (frames.callee() == nullptr) {
      // This contains only leaf frames (no next hop/callee).
      return;
    }
    auto new_frames = CalleeFrames::bottom();
    for (const auto& frame : frames) {
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
    frames = std::move(new_frames);
  });
}

void TaintV2::filter_invalid_frames(
    const std::function<bool(const Method*, const AccessPath&, const Kind*)>&
        is_valid) {
  map([&](CalleeFrames& frames) { frames.filter_invalid_frames(is_valid); });
}

bool TaintV2::contains_kind(const Kind* kind) const {
  return std::any_of(
      set_.begin(), set_.end(), [&](const CalleeFrames& callee_frames) {
        return callee_frames.contains_kind(kind);
      });
}

std::unordered_map<const Kind*, TaintV2> TaintV2::partition_by_kind() const {
  return partition_by_kind<const Kind*>([](const Kind* kind) { return kind; });
}

FeatureMayAlwaysSet TaintV2::features_joined() const {
  auto features = FeatureMayAlwaysSet::bottom();
  for (const auto& callee_frames : set_) {
    for (const auto& frame : callee_frames) {
      features.join_with(frame.features());
    }
  }
  return features;
}

void TaintV2::add(const CalleeFrames& frames) {
  set_.add(frames);
}

void TaintV2::map(const std::function<void(CalleeFrames&)>& f) {
  set_.map(f);
}

} // namespace marianatrench
