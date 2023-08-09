/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/CalleeFrames.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

CalleeProperties::CalleeProperties(
    const Method* MT_NULLABLE callee,
    CallInfo call_info)
    : callee_(callee), call_info_(call_info) {}

CalleeProperties::CalleeProperties(const TaintConfig& config)
    : callee_(config.callee()), call_info_(config.call_info()) {}

bool CalleeProperties::operator==(const CalleeProperties& other) const {
  return callee_ == other.callee_ && call_info_ == other.call_info_;
}

bool CalleeProperties::is_default() const {
  return callee_ == nullptr && call_info_.is_declaration();
}

void CalleeProperties::set_to_default() {
  callee_ = nullptr;
  call_info_ = CallInfo::declaration();
}

const Position* MT_NULLABLE
CallPositionFromTaintConfig::operator()(const TaintConfig& config) const {
  return config.call_position();
}

void CalleeFrames::add_local_position(const Position* position) {
  if (call_info().is_propagation()) {
    return; // Do not add local positions on propagations.
  }

  map_frames([position](CallPositionFrames frames) {
    frames.add_local_position(position);
    return frames;
  });
}

FeatureMayAlwaysSet CalleeFrames::locally_inferred_features(
    const Position* MT_NULLABLE position,
    const AccessPath& callee_port) const {
  auto result = FeatureMayAlwaysSet::bottom();
  result.join_with(
      frames_.get(position).locally_inferred_features(callee_port));
  return result;
}

CalleeFrames CalleeFrames::propagate(
    const Method* callee,
    const AccessPath& callee_port,
    const Position* call_position,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments)
    const {
  if (is_bottom()) {
    return CalleeFrames::bottom();
  }

  CallPositionFrames result;
  for (const auto& [_, call_position_frames] : frames_.bindings()) {
    result.join_with(call_position_frames.propagate(
        callee,
        callee_port,
        call_position,
        maximum_source_sink_distance,
        context,
        source_register_types,
        source_constant_arguments));
  }

  if (result.is_bottom()) {
    return CalleeFrames::bottom();
  }

  mt_assert(call_position == result.position());
  return CalleeFrames(
      CalleeProperties(callee, call_info().propagate()),
      FramesByKey{std::pair(call_position, result)});
}

CalleeFrames CalleeFrames::attach_position(const Position* position) const {
  CallPositionFrames result;

  // NOTE: It is not sufficient to simply update the key in the underlying
  // frames_ map. This functions similarly to `propagate`. Frame features are
  // propagated here, and we must call `CallPositionFrames::attach_position`
  // to ensure that.
  for (const auto& [_, call_position_frames] : frames_.bindings()) {
    result.join_with(call_position_frames.attach_position(position));
  }

  return CalleeFrames(
      // Since attaching the position creates a new leaf of the trace, we don't
      // respect the previous call info and instead default to origin.
      CalleeProperties(callee(), CallInfo::origin()),
      FramesByKey{std::pair(position, result)});
}

CalleeFrames CalleeFrames::apply_transform(
    const KindFactory& kind_factory,
    const TransformsFactory& transforms_factory,
    const UsedKinds& used_kinds,
    const TransformList* local_transforms) const {
  FramesByKey frames_by_call_position;

  for (const auto& [position, call_position_frames] : frames_.bindings()) {
    frames_by_call_position.set(
        position,
        call_position_frames.apply_transform(
            kind_factory, transforms_factory, used_kinds, local_transforms));
  }

  return CalleeFrames(properties_, frames_by_call_position);
}

void CalleeFrames::append_to_propagation_output_paths(
    Path::Element path_element) {
  if (!call_info().is_propagation()) {
    return;
  }

  map([path_element](Frame frame) {
    frame.append_to_propagation_output_paths(path_element);
    return frame;
  });
}

void CalleeFrames::update_maximum_collapse_depth(CollapseDepth collapse_depth) {
  if (!call_info().is_propagation()) {
    return;
  }

  map([collapse_depth](Frame frame) {
    frame.update_maximum_collapse_depth(collapse_depth);
    return frame;
  });
}

void CalleeFrames::update_non_leaf_positions(
    const std::function<
        const Position*(const Method*, const AccessPath&, const Position*)>&
        new_call_position,
    const std::function<LocalPositionSet(const LocalPositionSet&)>&
        new_local_positions) {
  const auto* callee = this->callee();
  if (callee == nullptr) {
    // This is a leaf.
    return;
  }

  FramesByKey result;
  for (const auto& [_, call_position_frames] : frames_.bindings()) {
    auto new_positions = call_position_frames.map_positions(
        [&new_call_position, callee](
            const auto& access_path, const auto* position) {
          return new_call_position(callee, access_path, position);
        },
        new_local_positions);

    for (const auto& [position, new_frames] : new_positions) {
      // Lambda below refuses to capture `new_frames` from a structured
      // binding, so explicitly declare it here.
      const auto& frames = new_frames;
      result.update(
          position, [&frames](CallPositionFrames* call_position_frames) {
            call_position_frames->join_with(frames);
          });
    }
  }

  frames_ = std::move(result);
}

Json::Value CalleeFrames::to_json(ExportOriginsMode export_origins_mode) const {
  auto taint = Json::Value(Json::arrayValue);
  for (const auto& [_, call_position_frames] : frames_.bindings()) {
    auto frames_json = call_position_frames.to_json(
        callee(), call_info(), export_origins_mode);
    mt_assert(frames_json.isArray());
    for (const auto& frame_json : frames_json) {
      taint.append(frame_json);
    }
  }
  return taint;
}

std::ostream& operator<<(std::ostream& out, const CalleeFrames& frames) {
  if (frames.is_top()) {
    return out << "T";
  } else {
    out << "[";
    for (const auto& [position, frames] : frames.frames_.bindings()) {
      out << "FramesByPosition(position=" << show(position) << ","
          << "frames=" << frames << "),";
    }
    return out << "]";
  }
}

} // namespace marianatrench
