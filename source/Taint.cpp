/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Constants.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Taint.h>

namespace marianatrench {

Taint::Taint(std::initializer_list<TaintConfig> configs) {
  for (const auto& config : configs) {
    add(config);
  }
}

TaintFramesIterator Taint::frames_iterator() const {
  return TaintFramesIterator(*this);
}

std::size_t Taint::num_frames() const {
  std::size_t count = 0;
  auto iterator = frames_iterator();
  std::for_each(iterator.begin(), iterator.end(), [&count](auto) { ++count; });
  return count;
}

void Taint::add(const TaintConfig& config) {
  set_.add(CalleeFrames{config});
}

void Taint::difference_with(const Taint& other) {
  set_.difference_with(other.set_);
}

void Taint::map(const std::function<void(Frame&)>& f) {
  set_.map([&](CalleeFrames& callee_frames) { callee_frames.map(f); });
}

void Taint::filter(const std::function<bool(const Frame&)>& predicate) {
  set_.map(
      [&](CalleeFrames& callee_frames) { callee_frames.filter(predicate); });
}

void Taint::set_leaf_origins_if_empty(const MethodSet& origins) {
  set_.map([&origins](CalleeFrames& frames) {
    if (frames.callee() == nullptr) {
      frames.set_origins_if_empty(origins);
    }
  });
}

void Taint::set_field_origins_if_empty_with_field_callee(const Field* field) {
  set_.map([field](CalleeFrames& frames) {
    // Setting a field callee must always be done on non-propagated leaves.
    mt_assert(frames.callee() == nullptr);
    frames.set_field_origins_if_empty_with_field_callee(field);
  });
}

void Taint::add_inferred_features(const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  set_.map([&features](CalleeFrames& frames) {
    frames.add_inferred_features(features);
  });
}

void Taint::add_local_position(const Position* position) {
  set_.map([position](CalleeFrames& frames) {
    frames.add_local_position(position);
  });
}

void Taint::set_local_positions(const LocalPositionSet& positions) {
  set_.map([&positions](CalleeFrames& frames) {
    frames.set_local_positions(positions);
  });
}

LocalPositionSet Taint::local_positions() const {
  auto result = LocalPositionSet::bottom();
  for (const auto& callee_frames : set_) {
    result.join_with(callee_frames.local_positions());
  }
  return result;
}

void Taint::add_inferred_features_and_local_position(
    const FeatureMayAlwaysSet& features,
    const Position* MT_NULLABLE position) {
  if (features.empty() && position == nullptr) {
    return;
  }

  set_.map([&features, position](CalleeFrames& frames) {
    frames.add_inferred_features_and_local_position(features, position);
  });
}

Taint Taint::propagate(
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
    result.set_.add(propagated);
  }
  return result;
}

Taint Taint::attach_position(const Position* position) const {
  Taint result;
  for (const auto& frames : set_) {
    result.set_.add(frames.attach_position(position));
  }
  return result;
}

void Taint::transform_kind_with_features(
    const std::function<std::vector<const Kind*>(const Kind*)>& transform_kind,
    const std::function<FeatureMayAlwaysSet(const Kind*)>& add_features) {
  set_.map([&transform_kind, &add_features](CalleeFrames& frames) {
    frames.transform_kind_with_features(transform_kind, add_features);
  });
}

Taint Taint::apply_transform(
    const Kinds& kinds,
    const Transforms& transforms,
    const UsedKinds& used_kinds,
    const TransformList* local_transforms) const {
  Taint result{};

  for (const auto& callee_frames : set_) {
    auto new_callee_frames = callee_frames.apply_transform(
        kinds, transforms, used_kinds, local_transforms);
    if (new_callee_frames.is_bottom()) {
      continue;
    }

    result.set_.add(new_callee_frames);
  }

  return result;
}

Json::Value Taint::to_json() const {
  auto taint = Json::Value(Json::arrayValue);
  for (const auto& frames : set_) {
    auto frames_json = frames.to_json();
    mt_assert(frames_json.isArray());
    for (const auto& frame_json : frames_json) {
      taint.append(frame_json);
    }
  }
  return taint;
}

std::ostream& operator<<(std::ostream& out, const Taint& taint) {
  return out << taint.set_;
}

void Taint::append_to_propagation_output_paths(Path::Element path_element) {
  set_.map([path_element](CalleeFrames& frames) {
    frames.append_to_propagation_output_paths(path_element);
  });
}

void Taint::update_non_leaf_positions(
    const std::function<
        const Position*(const Method*, const AccessPath&, const Position*)>&
        new_call_position,
    const std::function<LocalPositionSet(const LocalPositionSet&)>&
        new_local_positions) {
  set_.map([&new_call_position, &new_local_positions](CalleeFrames& frames) {
    frames.update_non_leaf_positions(new_call_position, new_local_positions);
  });
}

void Taint::filter_invalid_frames(
    const std::function<bool(const Method*, const AccessPath&, const Kind*)>&
        is_valid) {
  set_.map([&is_valid](CalleeFrames& frames) {
    frames.filter_invalid_frames(is_valid);
  });
}

bool Taint::contains_kind(const Kind* kind) const {
  return std::any_of(
      set_.begin(), set_.end(), [kind](const CalleeFrames& callee_frames) {
        return callee_frames.contains_kind(kind);
      });
}

std::unordered_map<const Kind*, Taint> Taint::partition_by_kind() const {
  return partition_by_kind<const Kind*>([](const Kind* kind) { return kind; });
}

FeatureMayAlwaysSet Taint::features_joined() const {
  auto features = FeatureMayAlwaysSet::bottom();
  for (const auto& callee_frames : set_) {
    for (const auto& frame : callee_frames) {
      features.join_with(frame.features());
    }
  }
  return features;
}

Taint Taint::propagation(PropagationConfig propagation) {
  return Taint{TaintConfig(
      /* kind */ propagation.kind(),
      /* callee_port */ AccessPath(propagation.propagation_kind()->root()),
      /* callee */ nullptr,
      /* field_callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ {},
      /* field_origins */ {},
      /* inferred_features */ propagation.inferred_features(),
      /* locally_inferred_features */ propagation.locally_inferred_features(),
      /* user_features */ propagation.user_features(),
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* canonical_names */ {},
      /* output_paths */ propagation.output_paths(),
      /* local_positions */ {},
      /* call_info */ CallInfo::Propagation)};
}

Taint Taint::propagation_taint(
    const PropagationKind* kind,
    PathTreeDomain output_paths,
    FeatureMayAlwaysSet inferred_features,
    FeatureSet user_features) {
  return Taint{TaintConfig(
      /* kind */ kind,
      /* callee_port */ AccessPath(kind->root()),
      /* callee */ nullptr,
      /* field_callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ {},
      /* field_origins */ {},
      /* inferred_features */ inferred_features,
      /* locally_inferred_features */ {},
      /* user_features */ user_features,
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* canonical_names */ {},
      /* output_paths */ output_paths,
      /* local_positions */ {},
      /* call_info */ CallInfo::Propagation)};
}

Taint Taint::essential() const {
  Taint result;
  for (const auto& frame : frames_iterator()) {
    auto callee_port = AccessPath(Root(Root::Kind::Return));

    // This is required by structure invariants.
    if (auto* propagation_kind =
            frame.kind()->discard_transforms()->as<PropagationKind>()) {
      callee_port = AccessPath(propagation_kind->root());
    }

    result.add(TaintConfig(
        /* kind */ frame.kind(),
        /* callee_port */ callee_port,
        /* callee */ nullptr,
        /* field_callee */ nullptr,
        /* call_position */ nullptr,
        /* distance */ 0,
        /* origins */ {},
        /* field_origins */ {},
        /* inferred_features */ FeatureMayAlwaysSet::bottom(),
        /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
        /* user_features */ FeatureSet::bottom(),
        /* via_type_of_ports */ {},
        /* via_value_of_ports */ {},
        /* canonical_names */ {},
        /* output_paths */ frame.output_paths(),
        /* local_positions */ {},
        /* call_info */ CallInfo::Declaration));
  }
  return result;
}

} // namespace marianatrench
