/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <iterator>
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

std::size_t Taint::num_frames() const {
  std::size_t count = 0;
  this->visit_frames([&count](const CallInfo&, const Frame&) { ++count; });
  return count;
}

void Taint::add(const LocalTaint& local_taint) {
  map_.update(
      local_taint.call_info(), [&local_taint](LocalTaint* existing) -> void {
        return existing->join_with(local_taint);
      });
}

void Taint::add(const TaintConfig& config) {
  add(LocalTaint{config});
}

void Taint::difference_with(const Taint& other) {
  map_.difference_like_operation(
      other.map_, [](LocalTaint* left, const LocalTaint& right) -> void {
        left->difference_with(right);
      });
}

void Taint::add_origins_if_declaration(
    const Method* method,
    const AccessPath* port) {
  map_.transform([method, port](LocalTaint* local_taint) {
    if (local_taint->callee() == nullptr &&
        !local_taint->call_kind().is_propagation_without_trace()) {
      local_taint->add_origins_if_declaration(method, port);
    }
  });
}

void Taint::add_origins_if_declaration(const Field* field) {
  map_.transform([field](LocalTaint* local_taint) -> void {
    // Setting a field callee must always be done on non-propagated leaves.
    mt_assert(local_taint->callee() == nullptr);
    local_taint->add_origins_if_declaration(field);
  });
}

void Taint::add_origins_if_declaration(std::string_view literal) {
  map_.transform([literal](LocalTaint* local_taint) -> void {
    // Setting a field callee must always be done on non-propagated leaves.
    mt_assert(local_taint->callee() == nullptr);
    local_taint->add_origins_if_declaration(literal);
  });
}

void Taint::add_locally_inferred_features(const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map_.transform([&features](LocalTaint* local_taint) -> void {
    local_taint->add_locally_inferred_features(features);
  });
}

void Taint::add_local_position(const Position* position) {
  map_.transform([position](LocalTaint* local_taint) -> void {
    local_taint->add_local_position(position);
  });
}

void Taint::set_local_positions(const LocalPositionSet& positions) {
  map_.transform([&positions](LocalTaint* local_taint) -> void {
    local_taint->set_local_positions(positions);
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
    const CallInfo& call_info) const {
  return map_.get(call_info).locally_inferred_features();
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
    const Method* MT_NULLABLE callee,
    const AccessPath* MT_NULLABLE callee_port,
    const Position* call_position,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    const CallClassIntervalContext& class_interval_context,
    const ClassIntervals::Interval& caller_class_interval,
    const RootPatriciaTreeAbstractPartition<FeatureSet>&
        add_features_to_arguments) const {
  Taint result;
  for (const auto& [_, local_taint] : map_.bindings()) {
    if (local_taint.call_kind().is_propagation_without_trace()) {
      // For propagation without traces, add as is.
      result.add(local_taint);
      continue;
    }

    auto propagated = local_taint.propagate(
        callee,
        callee_port,
        call_position,
        maximum_source_sink_distance,
        context,
        source_register_types,
        source_constant_arguments,
        class_interval_context,
        caller_class_interval,
        add_features_to_arguments);
    if (propagated.is_bottom()) {
      continue;
    }

    result.add(propagated);
  }
  return result;
}

Taint Taint::attach_position(const Position* position) const {
  Taint result;
  for (const auto& [_, local_taint] : map_.bindings()) {
    result.add(local_taint.attach_position(position));
  }
  return result;
}

Taint Taint::apply_transform(
    const KindFactory& kind_factory,
    const TransformsFactory& transforms_factory,
    const UsedKinds& used_kinds,
    const TransformList* local_transforms) const {
  Taint result{};

  for (const auto& [_, local_taint] : map_.bindings()) {
    auto new_callee_frames = local_taint.apply_transform(
        kind_factory, transforms_factory, used_kinds, local_transforms);
    if (new_callee_frames.is_bottom()) {
      continue;
    }

    result.add(new_callee_frames);
  }

  return result;
}

Taint Taint::update_with_propagation_trace(
    const CallInfo& propagation_call_info,
    const Frame& propagation_frame) const {
  Taint result;

  for (const auto& [_, local_taint] : map_.bindings()) {
    result.add(local_taint.update_with_propagation_trace(
        propagation_call_info, propagation_frame));
  }

  return result;
}

Taint Taint::from_json(const Json::Value& value, Context& context) {
  JsonValidation::null_or_array(value);

  Map call_info_to_local_taint;
  for (const auto& taint_json : value) {
    auto local_taint = LocalTaint::from_json(taint_json, context);
    call_info_to_local_taint.update(
        local_taint.call_info(), [&local_taint](LocalTaint* existing_taint) {
          existing_taint->join_with(local_taint);
        });
  }

  return Taint(call_info_to_local_taint);
}

Json::Value Taint::to_json(ExportOriginsMode export_origins_mode) const {
  auto taint = Json::Value(Json::arrayValue);
  for (const auto& [_, local_taint] : map_.bindings()) {
    taint.append(local_taint.to_json(export_origins_mode));
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
  map_.transform([path_element](LocalTaint* local_taint) -> void {
    local_taint->append_to_propagation_output_paths(path_element);
  });
}

void Taint::update_maximum_collapse_depth(CollapseDepth collapse_depth) {
  if (!collapse_depth.should_collapse()) {
    return;
  }

  map_.transform([collapse_depth](LocalTaint* local_taint) -> void {
    local_taint->update_maximum_collapse_depth(collapse_depth);
  });
}

Taint Taint::update_non_declaration_positions(
    const std::function<const Position*(
        const Method*,
        const AccessPath* MT_NULLABLE,
        const Position* MT_NULLABLE)>& new_call_position,
    const std::function<LocalPositionSet(const LocalPositionSet&)>&
        new_local_positions) const {
  Taint result;
  for (const auto& [_, local_taint] : map_.bindings()) {
    auto new_local_taints = local_taint.update_non_declaration_positions(
        new_call_position, new_local_positions);
    for (const auto& new_local_taint : new_local_taints) {
      result.add(new_local_taint);
    }
  }
  return result;
}

void Taint::filter_invalid_frames(const std::function<bool(
                                      const Method* MT_NULLABLE,
                                      const AccessPath* MT_NULLABLE,
                                      const Kind*)>& is_valid) {
  map_.transform([&is_valid](LocalTaint* local_taint) -> void {
    local_taint->filter_invalid_frames(is_valid);
  });
}

bool Taint::contains_kind(const Kind* kind) const {
  return std::any_of(
      map_.bindings().begin(),
      map_.bindings().end(),
      [kind](const std::pair<CallInfo, LocalTaint>& iterator) {
        return iterator.second.contains_kind(kind);
      });
}

std::unordered_map<const Kind*, Taint> Taint::partition_by_kind() const {
  return partition_by_kind<const Kind*>([](const Kind* kind) { return kind; });
}

std::vector<std::pair<const Kind*, Taint>> Taint::sorted_partition_by_kind()
    const {
  auto partitioned_by_kind =
      partition_by_kind<const Kind*>([](const Kind* kind) { return kind; });

  std::vector<std::pair<const Kind*, Taint>> result{
      std::make_move_iterator(partitioned_by_kind.begin()),
      std::make_move_iterator(partitioned_by_kind.end())};

  std::sort(
      result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.first->to_trace_string() < right.first->to_trace_string();
      });

  return result;
}

void Taint::intersect_intervals_with(const Taint& other) {
  std::unordered_set<CallClassIntervalContext> other_intervals;

  // Using an exception to break out of the loop early since `visit_frames`
  // does not allow us to do that.
  class frame_does_not_preserve_type_context {};
  try {
    other.visit_frames([&other_intervals](
                           const CallInfo&, const Frame& other_frame) {
      const auto& other_frame_interval = other_frame.class_interval_context();
      // All frames in `this` will intersect with a frame in `other` that does
      // not preserve type context.
      if (!other_frame_interval.preserves_type_context()) {
        throw frame_does_not_preserve_type_context();
      }

      other_intervals.insert(other_frame_interval);
    });
  } catch (const frame_does_not_preserve_type_context&) {
    return;
  }

  // Keep only frames that intersect with some interval in `other`.
  // Frames that do not preserve type context are considered to intersect with
  // everything.
  filter_frames([&other_intervals](const Frame& frame) {
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
  for (const auto& [_, local_taint] : map_.bindings()) {
    features.join_with(local_taint.features_joined());
  }
  return features;
}

std::unordered_set<const Kind*> Taint::kinds() const {
  std::unordered_set<const Kind*> result;
  visit_kind_frames([&result](const KindFrames& kind_frames) {
    result.insert(kind_frames.kind());
  });
  return result;
}

void Taint::collapse_class_intervals() {
  transform_kind_frames([](KindFrames kind_frames) {
    kind_frames.collapse_class_intervals();
    return kind_frames;
  });
}

Taint Taint::propagation(PropagationConfig propagation) {
  return Taint{TaintConfig(
      /* kind */ propagation.kind(),
      /* callee_port */ propagation.callee_port(),
      /* callee */ nullptr,
      /* call_kind */ propagation.call_kind(),
      /* call_position */ nullptr,
      /* class_interval_context */ CallClassIntervalContext(),
      /* distance */ 0,
      /* origins */ {},
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
      /* callee_port */
      AccessPathFactory::singleton().get(AccessPath(kind->root())),
      /* callee */ nullptr,
      /* call_kind */ CallKind::propagation(),
      /* call_position */ nullptr,
      /* class_interval_context */ CallClassIntervalContext(),
      /* distance */ 0,
      /* origins */ {},
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
  const auto* return_callee_port =
      AccessPathFactory::singleton().get(AccessPath(Root(Root::Kind::Return)));
  this->visit_frames(
      [&result, return_callee_port](const CallInfo&, const Frame& frame) {
        const auto* callee_port = return_callee_port;
        CallKind call_kind = CallKind::declaration();

        // This is required by structure invariants.
        if (auto* propagation_kind =
                frame.kind()->discard_transforms()->as<PropagationKind>()) {
          callee_port = AccessPathFactory::singleton().get(
              AccessPath(propagation_kind->root()));
          call_kind = CallKind::propagation();
        }

        result.add(TaintConfig(
            /* kind */ frame.kind(),
            /* callee_port */ callee_port,
            /* callee */ nullptr,
            /* call_kind */ call_kind,
            /* call_position */ nullptr,
            /* class_interval_context */ CallClassIntervalContext(),
            /* distance */ 0,
            /* origins */ {},
            /* inferred_features */ FeatureMayAlwaysSet::bottom(),
            /* user_features */ FeatureSet::bottom(),
            /* via_type_of_ports */ {},
            /* via_value_of_ports */ {},
            /* canonical_names */ {},
            /* output_paths */ frame.output_paths(),
            /* local_positions */ {},
            /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
            /* extra_traces */ {}));
      });
  return result;
}

} // namespace marianatrench
