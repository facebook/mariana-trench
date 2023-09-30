/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/CallPositionFrames.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

CallPositionProperties::CallPositionProperties(
    const Position* MT_NULLABLE position)
    : position_(position) {}

CallPositionProperties::CallPositionProperties(const TaintConfig& config)
    : position_(config.call_position()) {}

FeatureMayAlwaysSet CallPositionFrames::locally_inferred_features(
    const AccessPath& callee_port) const {
  auto result = FeatureMayAlwaysSet::bottom();
  for (const auto& [_, callee_port_frames] : frames_.bindings()) {
    if (callee_port_frames.callee_port() == callee_port) {
      result.join_with(callee_port_frames.locally_inferred_features());
      break;
    }
  }
  return result;
}

AccessPath CalleePortFromTaintConfig::operator()(
    const TaintConfig& config) const {
  return config.callee_port();
}

void CallPositionFrames::add_local_position(const Position* position) {
  map_frames([position](CalleePortFrames* callee_port_frames) -> void {
    callee_port_frames->add_local_position(position);
  });
}

CallPositionFrames CallPositionFrames::propagate(
    const Method* callee,
    const AccessPath& callee_port,
    const Position* call_position,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    const CallClassIntervalContext& class_interval_context,
    const ClassIntervals::Interval& caller_class_interval) const {
  if (is_bottom()) {
    return CallPositionFrames::bottom();
  }

  // In most cases, the propagated callee_port should be the given `callee_port`
  // argument, in which case, simply joining all propagated CalleePortFrames
  // will suffice. However, CRTEX leaves can produce different canonical
  // callee_ports which prevents us from taking the easier route.
  FramesByKey result;
  for (const auto& [_, callee_port_frames] : frames_.bindings()) {
    auto propagated = callee_port_frames.propagate(
        callee,
        callee_port,
        call_position,
        maximum_source_sink_distance,
        context,
        source_register_types,
        source_constant_arguments,
        class_interval_context,
        caller_class_interval);
    result.update(
        propagated.callee_port(), [&propagated](CalleePortFrames* frames) {
          frames->join_with(propagated);
        });
  }

  if (result.is_bottom()) {
    return CallPositionFrames::bottom();
  }

  return CallPositionFrames(CallPositionProperties(call_position), result);
}

CallPositionFrames CallPositionFrames::update_with_propagation_trace(
    const Frame& propagation_frame) const {
  if (is_bottom()) {
    return CallPositionFrames::bottom();
  }

  mt_assert(position() == nullptr);

  FramesByKey result;
  for (const auto& [_, callee_port_frames] : frames_.bindings()) {
    auto propagated =
        callee_port_frames.update_with_propagation_trace(propagation_frame);
    mt_assert(propagated.callee_port() == propagation_frame.callee_port());

    result.update(
        propagated.callee_port(), [&propagated](CalleePortFrames* frames) {
          frames->join_with(propagated);
        });
  }

  return CallPositionFrames(
      CallPositionProperties(propagation_frame.call_position()), result);
}

CallPositionFrames CallPositionFrames::attach_position(
    const Position* position) const {
  FramesByKey result;

  // NOTE: This method does more than update the position in frames. It
  // functions similarly to `propagate`. Frame features are propagated here.
  for (const auto& [_, callee_port_frames] : frames_.bindings()) {
    for (const auto& frame : callee_port_frames) {
      if (!frame.is_leaf()) {
        continue;
      }

      auto inferred_features = frame.features();
      inferred_features.add(callee_port_frames.locally_inferred_features());
      auto local_positions = callee_port_frames.local_positions();
      result.update(
          frame.callee_port(),
          [position, &frame, &inferred_features, &local_positions](
              CalleePortFrames* frames) {
            frames->join_with(CalleePortFrames{TaintConfig(
                frame.kind(),
                frame.callee_port(),
                /* callee */ nullptr,
                CallInfo::origin(),
                /* field_callee */ nullptr,
                /* call_position */ position,
                // TODO(T158171922): Re-visit what the appropriate interval
                // should be when implementing class intervals.
                frame.class_interval_context(),
                /* distance */ 0,
                frame.origins(),
                frame.field_origins(),
                inferred_features,
                /* user_features */ FeatureSet::bottom(),
                /* via_type_of_ports */ {},
                /* via_value_of_ports */ {},
                frame.canonical_names(),
                /* output_paths */ {},
                local_positions,
                // Since CallPositionFrames::attach_position is used (only) for
                // parameter_sinks and return sources which may be included in
                // an issue as a leaf, we need to make sure that those leaf
                // frames in issues contain the user_features as being locally
                // inferred.
                /* locally_inferred_features */
                frame.user_features().is_bottom()
                    ? FeatureMayAlwaysSet::bottom()
                    : FeatureMayAlwaysSet::make_always(frame.user_features()),
                frame.extra_traces())});
          });
    }
  }

  return CallPositionFrames(CallPositionProperties(position), result);
}

CallPositionFrames CallPositionFrames::apply_transform(
    const KindFactory& kind_factory,
    const TransformsFactory& transforms_factory,
    const UsedKinds& used_kinds,
    const TransformList* local_transforms) const {
  FramesByKey frames_by_callee_port;

  for (const auto& [callee_port_hash, callee_port_frames] :
       frames_.bindings()) {
    frames_by_callee_port.set(
        callee_port_hash,
        callee_port_frames.apply_transform(
            kind_factory, transforms_factory, used_kinds, local_transforms));
  }

  return CallPositionFrames{properties_, frames_by_callee_port};
}

std::unordered_map<const Position*, CallPositionFrames>
CallPositionFrames::map_positions(
    const std::function<const Position*(const AccessPath&, const Position*)>&
        new_call_position,
    const std::function<LocalPositionSet(const LocalPositionSet&)>&
        new_local_positions) const {
  std::unordered_map<const Position*, CallPositionFrames> result;
  for (const auto& [callee_port_hash, callee_port_frames] :
       frames_.bindings()) {
    auto call_position = new_call_position(
        callee_port_frames.callee_port(), properties_.position());
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
          frame.call_info(),
          frame.field_callee(),
          call_position,
          frame.class_interval_context(),
          frame.distance(),
          frame.origins(),
          frame.field_origins(),
          frame.inferred_features(),
          frame.user_features(),
          frame.via_type_of_ports(),
          frame.via_value_of_ports(),
          frame.canonical_names(),
          /* output_paths */ {},
          /* local_positions */ {},
          callee_port_frames.locally_inferred_features(),
          frame.extra_traces());
      new_frames.add(new_frame);
    }

    if (!new_frames.is_bottom()) {
      new_frames.set_local_positions(local_positions);
    }

    auto found = result.find(call_position);
    auto frames = CallPositionFrames(
        CallPositionProperties(call_position),
        FramesByKey{std::pair(callee_port_hash, new_frames)});
    if (found != result.end()) {
      found->second.join_with(frames);
    } else {
      result.emplace(call_position, frames);
    }
  }
  return result;
}

Json::Value CallPositionFrames::to_json(
    const Method* MT_NULLABLE callee,
    CallInfo call_info,
    ExportOriginsMode export_origins_mode) const {
  auto taint = Json::Value(Json::arrayValue);
  for (const auto& [_, callee_port_frames] : frames_.bindings()) {
    auto frames_json = callee_port_frames.to_json(
        callee, properties_.position(), call_info, export_origins_mode);
    taint.append(frames_json);
  }
  return taint;
}

std::ostream& operator<<(std::ostream& out, const CallPositionFrames& frames) {
  // No representation for top() because FramesByCalleePort::top() is N/A.
  out << "[";
  for (const auto& [_, callee_port_frames] : frames.frames_.bindings()) {
    out << "FramesByCalleePort(" << show(callee_port_frames) << "),";
  }
  return out << "]";
}

} // namespace marianatrench
