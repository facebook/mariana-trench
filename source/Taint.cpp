/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <unordered_set>

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

void Taint::add(const CalleeFrames& callee_frames) {
  map_.update(
      callee_frames.callee_frame_key(),
      [&callee_frames](const CalleeFrames& existing) {
        return existing.join(callee_frames);
      });
}

void Taint::add(const TaintConfig& config) {
  add(CalleeFrames{config});
}

void Taint::difference_with(const Taint& other) {
  map_.difference_like_operation(
      other.map_, [](const CalleeFrames& left, const CalleeFrames& right) {
        auto left_copy = left;
        left_copy.difference_with(right);
        return left_copy;
      });
}

void Taint::set_leaf_origins_if_empty(const MethodSet& origins) {
  map_.map([&origins](CalleeFrames frames) {
    if (frames.callee() == nullptr &&
        !frames.call_kind().is_propagation_without_trace()) {
      frames.set_origins_if_empty(origins);
    }
    return frames;
  });
}

void Taint::set_field_origins_if_empty_with_field_callee(const Field* field) {
  map_.map([field](CalleeFrames frames) {
    // Setting a field callee must always be done on non-propagated leaves.
    mt_assert(frames.callee() == nullptr);
    frames.set_field_origins_if_empty_with_field_callee(field);
    return frames;
  });
}

void Taint::add_locally_inferred_features(const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map_.map([&features](CalleeFrames frames) {
    frames.add_locally_inferred_features(features);
    return frames;
  });
}

void Taint::add_local_position(const Position* position) {
  map_.map([position](CalleeFrames frames) {
    frames.add_local_position(position);
    return frames;
  });
}

void Taint::set_local_positions(const LocalPositionSet& positions) {
  map_.map([&positions](CalleeFrames frames) {
    frames.set_local_positions(positions);
    return frames;
  });
}

LocalPositionSet Taint::local_positions() const {
  auto result = LocalPositionSet::bottom();
  for (const auto& [_, callee_frames] : map_.bindings()) {
    result.join_with(callee_frames.local_positions());
  }
  return result;
}

FeatureMayAlwaysSet Taint::locally_inferred_features(
    const Method* MT_NULLABLE callee,
    CallKind call_kind,
    const Position* MT_NULLABLE position,
    const AccessPath& callee_port) const {
  auto result = FeatureMayAlwaysSet::bottom();
  for (const auto& [_, callee_frames] : map_.bindings()) {
    // Key look up by callee will be nice but is not supported by underlying
    // GroupHashedSetAbstractDomain.
    if (callee_frames.callee() == callee &&
        callee_frames.call_kind() == call_kind) {
      result.join_with(
          callee_frames.locally_inferred_features(position, callee_port));
      break;
    }
  }
  return result;
}

void Taint::add_locally_inferred_features_and_local_position(
    const FeatureMayAlwaysSet& features,
    const Position* MT_NULLABLE position) {
  if (features.empty() && position == nullptr) {
    return;
  }

  add_locally_inferred_features(features);
  if (position != nullptr) {
    add_local_position(position);
  }
}

Taint Taint::propagate(
    const Method* callee,
    const AccessPath& callee_port,
    const Position* call_position,
    int maximum_source_sink_distance,
    const FeatureMayAlwaysSet& extra_features,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    const CallClassIntervalContext& class_interval_context,
    const ClassIntervals::Interval& caller_class_interval) const {
  Taint result;
  for (const auto& [_, frames] : map_.bindings()) {
    if (frames.call_kind().is_propagation_without_trace()) {
      // For propagation without traces, add as is.
      result.add(frames);
      continue;
    }

    auto propagated = frames.propagate(
        callee,
        callee_port,
        call_position,
        maximum_source_sink_distance,
        context,
        source_register_types,
        source_constant_arguments,
        class_interval_context,
        caller_class_interval);
    if (propagated.is_bottom()) {
      continue;
    }

    propagated.add_locally_inferred_features(extra_features);
    result.add(propagated);
  }
  return result;
}

Taint Taint::attach_position(const Position* position) const {
  Taint result;
  for (const auto& [_, frames] : map_.bindings()) {
    result.add(frames.attach_position(position));
  }
  return result;
}

Taint Taint::apply_transform(
    const KindFactory& kind_factory,
    const TransformsFactory& transforms_factory,
    const UsedKinds& used_kinds,
    const TransformList* local_transforms) const {
  Taint result{};

  for (const auto& [_, callee_frames] : map_.bindings()) {
    auto new_callee_frames = callee_frames.apply_transform(
        kind_factory, transforms_factory, used_kinds, local_transforms);
    if (new_callee_frames.is_bottom()) {
      continue;
    }

    result.add(new_callee_frames);
  }

  return result;
}

Taint Taint::update_with_propagation_trace(
    const Frame& propagation_frame) const {
  Taint result;

  for (const auto& [_, callee_frame] : map_.bindings()) {
    result.add(callee_frame.update_with_propagation_trace(propagation_frame));
  }

  return result;
}

Json::Value Taint::to_json(ExportOriginsMode export_origins_mode) const {
  auto taint = Json::Value(Json::arrayValue);
  for (const auto& [_, frames] : map_.bindings()) {
    auto frames_json = frames.to_json(export_origins_mode);
    mt_assert(frames_json.isArray());
    for (const auto& frame_json : frames_json) {
      taint.append(frame_json);
    }
  }
  return taint;
}

std::ostream& operator<<(std::ostream& out, const Taint& taint) {
  out << "{";
  for (auto it = taint.map_.bindings().begin();
       it != taint.map_.bindings().end();) {
    out << it->second;
    ++it;
    if (it != taint.map_.bindings().end()) {
      out << ", ";
    }
  }
  return out << "}";
}

void Taint::append_to_propagation_output_paths(Path::Element path_element) {
  map_.map([path_element](CalleeFrames frames) {
    frames.append_to_propagation_output_paths(path_element);
    return frames;
  });
}

void Taint::update_maximum_collapse_depth(CollapseDepth collapse_depth) {
  if (!collapse_depth.should_collapse()) {
    return;
  }

  map_.map([collapse_depth](CalleeFrames frames) {
    frames.update_maximum_collapse_depth(collapse_depth);
    return frames;
  });
}

void Taint::update_non_leaf_positions(
    const std::function<
        const Position*(const Method*, const AccessPath&, const Position*)>&
        new_call_position,
    const std::function<LocalPositionSet(const LocalPositionSet&)>&
        new_local_positions) {
  map_.map([&new_call_position, &new_local_positions](CalleeFrames frames) {
    frames.update_non_leaf_positions(new_call_position, new_local_positions);
    return frames;
  });
}

void Taint::filter_invalid_frames(
    const std::function<bool(const Method*, const AccessPath&, const Kind*)>&
        is_valid) {
  map_.map([&is_valid](CalleeFrames frames) {
    frames.filter_invalid_frames(is_valid);
    return frames;
  });
}

bool Taint::contains_kind(const Kind* kind) const {
  return std::any_of(
      map_.bindings().begin(),
      map_.bindings().end(),
      [kind](const std::pair<CalleeFrames::Key, CalleeFrames>& iterator) {
        return iterator.second.contains_kind(kind);
      });
}

std::unordered_map<const Kind*, Taint> Taint::partition_by_kind() const {
  return partition_by_kind<const Kind*>([](const Kind* kind) { return kind; });
}

void Taint::intersect_intervals_with(const Taint& other) {
  std::unordered_set<CallClassIntervalContext> other_intervals;
  for (const auto& other_frame : other.frames_iterator()) {
    const auto& other_frame_interval = other_frame.class_interval_context();
    // All frames in `this` will intersect with a frame in `other` that does not
    // preserve type context.
    if (!other_frame_interval.preserves_type_context()) {
      return;
    }

    other_intervals.insert(other_frame_interval);
  }

  // Keep only frames that intersect with some interval in `other`.
  // Frames that do not preserve type context are considered to intersect with
  // everything.
  filter([&other_intervals](const Frame& frame) {
    const auto& frame_interval = frame.class_interval_context();
    if (!frame_interval.preserves_type_context()) {
      return true;
    }

    bool intersects_with_some_other_frame = std::any_of(
        other_intervals.begin(),
        other_intervals.end(),
        [&frame_interval](const auto& other_frame_interval) {
          return !other_frame_interval.callee_interval()
                      .meet(frame_interval.callee_interval())
                      .is_bottom();
        });
    return intersects_with_some_other_frame;
  });
}

FeatureMayAlwaysSet Taint::features_joined() const {
  auto features = FeatureMayAlwaysSet::bottom();
  for (const auto& [_, callee_frames] : map_.bindings()) {
    features.join_with(callee_frames.features_joined());
  }
  return features;
}

Taint Taint::propagation(PropagationConfig propagation) {
  return Taint{TaintConfig(
      /* kind */ propagation.kind(),
      /* callee_port */ propagation.callee_port(),
      /* callee */ nullptr,
      /* call_kind */ propagation.call_kind(),
      /* field_callee */ nullptr,
      /* call_position */ nullptr,
      /* class_interval_context */ CallClassIntervalContext(),
      /* distance */ 0,
      /* origins */ {},
      /* field_origins */ {},
      /* inferred_features */ propagation.inferred_features(),
      /* user_features */ propagation.user_features(),
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* canonical_names */ {},
      /* output_paths */ propagation.output_paths(),
      /* local_positions */ {},
      /* locally_inferred_features */ propagation.locally_inferred_features(),
      /* extra_traces */ {})};
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
      /* call_kind */ CallKind::propagation(),
      /* field_callee */ nullptr,
      /* call_position */ nullptr,
      /* class_interval_context */ CallClassIntervalContext(),
      /* distance */ 0,
      /* origins */ {},
      /* field_origins */ {},
      /* inferred_features */ inferred_features,
      /* user_features */ user_features,
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* canonical_names */ {},
      /* output_paths */ output_paths,
      /* local_positions */ {},
      /* locally_inferred_features */ {},
      /* extra_traces */ {})};
}

Taint Taint::essential() const {
  Taint result;
  for (const auto& frame : frames_iterator()) {
    auto callee_port = AccessPath(Root(Root::Kind::Return));
    CallKind call_kind = CallKind::declaration();

    // This is required by structure invariants.
    if (auto* propagation_kind =
            frame.kind()->discard_transforms()->as<PropagationKind>()) {
      callee_port = AccessPath(propagation_kind->root());
      call_kind = CallKind::propagation();
    }

    result.add(TaintConfig(
        /* kind */ frame.kind(),
        /* callee_port */ callee_port,
        /* callee */ nullptr,
        /* call_kind */ call_kind,
        /* field_callee */ nullptr,
        /* call_position */ nullptr,
        /* class_interval_context */ CallClassIntervalContext(),
        /* distance */ 0,
        /* origins */ {},
        /* field_origins */ {},
        /* inferred_features */ FeatureMayAlwaysSet::bottom(),
        /* user_features */ FeatureSet::bottom(),
        /* via_type_of_ports */ {},
        /* via_value_of_ports */ {},
        /* canonical_names */ {},
        /* output_paths */ frame.output_paths(),
        /* local_positions */ {},
        /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
        /* extra_traces */ {}));
  }
  return result;
}

} // namespace marianatrench
