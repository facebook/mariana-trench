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

void TaintV2::add(const CalleeFrames& frames) {
  set_.add(frames);
}

void TaintV2::map(const std::function<void(CalleeFrames&)>& f) {
  set_.map(f);
}

} // namespace marianatrench
