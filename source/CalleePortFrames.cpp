/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/CalleePortFrames.h>
#include <mariana-trench/ExtraTraceSet.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/Heuristics.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/PathTreeDomain.h>

namespace marianatrench {

CalleePortFrames::CalleePortFrames(std::initializer_list<TaintConfig> configs)
    : callee_port_(Root(Root::Kind::Leaf)),
      frames_(FramesByKind::bottom()),
      locally_inferred_features_(FeatureMayAlwaysSet::bottom()) {
  for (const auto& config : configs) {
    add(config);
  }
}

CalleePortFrames::CalleePortFrames(const Frame& frame) : CalleePortFrames() {
  if (!frame.is_bottom()) {
    add(frame);
  }
}

void CalleePortFrames::add(const TaintConfig& config) {
  if (is_bottom()) {
    callee_port_ = config.callee_port();
  } else {
    mt_assert(callee_port_ == config.callee_port());
  }

  local_positions_.join_with(config.local_positions());
  locally_inferred_features_.join_with(config.locally_inferred_features());
  frames_.update(config.kind(), [&config](const KindFrames& frames) {
    auto frames_copy = frames;
    frames_copy.add(config);
    return frames_copy;
  });
}

bool CalleePortFrames::leq(const CalleePortFrames& other) const {
  if (is_bottom()) {
    return true;
  } else if (other.is_bottom()) {
    return false;
  }
  mt_assert(has_same_key(other));
  return frames_.leq(other.frames_) &&
      local_positions_.leq(other.local_positions_) &&
      locally_inferred_features_.leq(other.locally_inferred_features_);
}

bool CalleePortFrames::equals(const CalleePortFrames& other) const {
  mt_assert(is_bottom() || other.is_bottom() || has_same_key(other));
  return frames_.equals(other.frames_) &&
      local_positions_.equals(other.local_positions_) &&
      locally_inferred_features_.equals(other.locally_inferred_features_);
}

void CalleePortFrames::join_with(const CalleePortFrames& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    callee_port_ = other.callee_port();
  }
  mt_assert(other.is_bottom() || has_same_key(other));

  frames_.join_with(other.frames_);
  local_positions_.join_with(other.local_positions_);
  locally_inferred_features_.join_with(other.locally_inferred_features_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void CalleePortFrames::widen_with(const CalleePortFrames& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    callee_port_ = other.callee_port();
  }
  mt_assert(other.is_bottom() || has_same_key(other));

  frames_.widen_with(other.frames_);
  local_positions_.widen_with(other.local_positions_);
  locally_inferred_features_.widen_with(other.locally_inferred_features_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void CalleePortFrames::meet_with(const CalleePortFrames& other) {
  if (is_bottom()) {
    callee_port_ = other.callee_port();
  }
  mt_assert(other.is_bottom() || has_same_key(other));

  frames_.meet_with(other.frames_);
  if (frames_.is_bottom()) {
    set_to_bottom();
  } else {
    local_positions_.meet_with(other.local_positions_);
    locally_inferred_features_.meet_with(other.locally_inferred_features_);
  }
}

void CalleePortFrames::narrow_with(const CalleePortFrames& other) {
  if (is_bottom()) {
    callee_port_ = other.callee_port();
  }
  mt_assert(other.is_bottom() || has_same_key(other));

  frames_.narrow_with(other.frames_);
  if (frames_.is_bottom()) {
    set_to_bottom();
  } else {
    local_positions_.narrow_with(other.local_positions_);
    locally_inferred_features_.narrow_with(other.locally_inferred_features_);
  }
}

void CalleePortFrames::difference_with(const CalleePortFrames& other) {
  if (is_bottom()) {
    callee_port_ = other.callee_port();
  }
  mt_assert(other.is_bottom() || has_same_key(other));

  // For properties that apply to all frames, if LHS is not leq RHS, do not
  // apply the difference operator to the frames because every frame on LHS
  // would not be considered leq its RHS frame.
  if (local_positions_.leq(other.local_positions_) &&
      locally_inferred_features_.leq(other.locally_inferred_features_)) {
    frames_.difference_like_operation(
        other.frames_, [](const KindFrames& left, const KindFrames& right) {
          auto left_copy = left;
          left_copy.difference_with(right);
          return left_copy;
        });
    if (frames_.is_bottom()) {
      set_to_bottom();
    }
  }
}

void CalleePortFrames::set_origins_if_empty(const MethodSet& origins) {
  map([&origins](Frame frame) {
    if (frame.origins().empty()) {
      frame.set_origins(origins);
    }
    return frame;
  });
}

void CalleePortFrames::set_field_origins_if_empty(const Field* field) {
  map([field](Frame frame) {
    if (frame.field_origins().empty()) {
      frame.set_field_origins(FieldSet{field});
    }
    return frame;
  });
}

void CalleePortFrames::add_locally_inferred_features(
    const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  locally_inferred_features_.add(features);
}

void CalleePortFrames::add_local_position(const Position* position) {
  local_positions_.add(position);
}

void CalleePortFrames::set_local_positions(LocalPositionSet positions) {
  local_positions_ = std::move(positions);
}

void CalleePortFrames::append_to_propagation_output_paths(
    Path::Element path_element) {
  map([path_element](Frame frame) {
    frame.append_to_propagation_output_paths(path_element);
    return frame;
  });
}

CalleePortFrames CalleePortFrames::propagate(
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
    return CalleePortFrames::bottom();
  }

  // CRTEX is identified by the "anchor" port, leaf-ness is identified by the
  // path() length. Once a CRTEX frame is propagated, its path is never empty.
  bool is_crtex_leaf =
      callee_port_.root().is_anchor() && callee_port_.path().size() == 0;
  auto propagated_callee_port =
      is_crtex_leaf ? callee_port.canonicalize_for_method(callee) : callee_port;
  FramesByKind propagated_frames_by_kind;
  for (const auto& [kind, frames] : frames_.bindings()) {
    auto propagated = frames.propagate(
        callee,
        propagated_callee_port,
        call_position,
        locally_inferred_features_,
        maximum_source_sink_distance,
        context,
        source_register_types,
        source_constant_arguments,
        class_interval_context,
        caller_class_interval);

    if (!propagated.is_bottom()) {
      propagated_frames_by_kind.update(
          propagated.kind(), [&propagated](const KindFrames& previous) {
            return previous.join(propagated);
          });
    }
  }

  if (propagated_frames_by_kind.is_bottom()) {
    return CalleePortFrames::bottom();
  }

  return CalleePortFrames(
      propagated_callee_port,
      propagated_frames_by_kind,
      /* local_positions */ {},
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom());
}

CalleePortFrames CalleePortFrames::update_with_propagation_trace(
    const Frame& propagation_frame) const {
  FramesByKind frames_by_kind{};
  for (const auto& frame : *this) {
    auto new_frame = frame.update_with_propagation_trace(propagation_frame);
    if (!new_frame.is_bottom()) {
      frames_by_kind.update(
          new_frame.kind(), [&new_frame](const KindFrames& old_frames) {
            auto frames_copy = old_frames;
            frames_copy.add(new_frame);
            return frames_copy;
          });
    }
  }

  mt_assert(!frames_by_kind.is_bottom());

  return CalleePortFrames(
      propagation_frame.callee_port(),
      frames_by_kind,
      local_positions_,
      locally_inferred_features_);
}

CalleePortFrames CalleePortFrames::apply_transform(
    const KindFactory& kind_factory,
    const TransformsFactory& transforms,
    const UsedKinds& used_kinds,
    const TransformList* local_transforms) const {
  FramesByKind new_frames;
  for (const auto& frame : *this) {
    auto new_frame = frame.apply_transform(
        kind_factory, transforms, used_kinds, local_transforms);
    if (!new_frame.is_bottom()) {
      new_frames.update(
          new_frame.kind(), [&new_frame](const KindFrames& old_frames) {
            auto frames_copy = old_frames;
            frames_copy.add(new_frame);
            return frames_copy;
          });
    }
  }

  if (new_frames.is_bottom()) {
    return CalleePortFrames::bottom();
  }

  return CalleePortFrames(
      callee_port_, new_frames, local_positions_, locally_inferred_features_);
}

void CalleePortFrames::filter_invalid_frames(
    const std::function<bool(const Method*, const AccessPath&, const Kind*)>&
        is_valid) {
  FramesByKind new_frames;
  for (const auto& [kind, kind_frames] : frames_.bindings()) {
    auto kind_frames_copy = kind_frames;
    kind_frames_copy.filter_invalid_frames(is_valid);
    if (kind_frames_copy != KindFrames::bottom()) {
      new_frames.set(kind, kind_frames_copy);
    }
  }

  if (new_frames.is_bottom()) {
    set_to_bottom();
  } else {
    frames_ = std::move(new_frames);
  }
}

bool CalleePortFrames::contains_kind(const Kind* kind) const {
  for (const auto& [actual_kind, _] : frames_.bindings()) {
    if (actual_kind == kind) {
      return true;
    }
  }
  return false;
}

FeatureMayAlwaysSet CalleePortFrames::features_joined() const {
  auto features = FeatureMayAlwaysSet::bottom();
  for (const auto& frame : *this) {
    auto combined_features = frame.features();
    combined_features.add(locally_inferred_features_);
    features.join_with(combined_features);
  }
  return features;
}

std::ostream& operator<<(std::ostream& out, const CalleePortFrames& frames) {
  mt_assert(!frames.frames_.is_top());
  out << "CalleePortFrames(callee_port=" << frames.callee_port();

  const auto& local_positions = frames.local_positions();
  if (!local_positions.is_bottom() && !local_positions.empty()) {
    out << ", local_positions=" << frames.local_positions();
  }

  const auto& locally_inferred_features = frames.locally_inferred_features();
  if (!locally_inferred_features.is_bottom() &&
      !locally_inferred_features.empty()) {
    out << ", locally_inferred_features=" << locally_inferred_features;
  }

  out << ", frames=[";
  for (const auto& [kind, frame] : frames.frames_.bindings()) {
    out << show(frame) << ",";
  }
  return out << "])";
}

void CalleePortFrames::add(const Frame& frame) {
  if (is_bottom()) {
    callee_port_ = frame.callee_port();
  } else {
    mt_assert(callee_port_ == frame.callee_port());
  }

  frames_.update(frame.kind(), [&frame](const KindFrames& old_frames) {
    auto frames_copy = old_frames;
    frames_copy.add(frame);
    return frames_copy;
  });
}

Json::Value CalleePortFrames::to_json(
    const Method* MT_NULLABLE callee,
    const Position* MT_NULLABLE position,
    CallKind call_kind,
    ExportOriginsMode export_origins_mode) const {
  auto taint = Json::Value(Json::objectValue);

  auto kinds = Json::Value(Json::arrayValue);
  for (const auto& frame : *this) {
    kinds.append(frame.to_json(export_origins_mode));
  }
  taint["kinds"] = kinds;

  // In most cases, all 3 values (callee, position, port) are expected to be
  // present. Some edge cases are:
  //
  // - Standard leaf/terminal frames: The "call" key will be absent because
  //   there is no "next hop".
  // - CRTEX leaf/terminal frames: The callee port will be "producer/anchor".
  //   SAPP post-processing will transform it to something that other static
  //   analysis tools in the family can understand.
  // - Return sinks and parameter sources: There is no "callee", but the
  //   position points to the return instruction/parameter.

  // We don't want to emit calls in origin frames in the non-CRTEX case.
  if (!callee_port_.root().is_leaf_port() && call_kind.is_origin()) {
    // Since we don't emit calls for origins, we need to provide the origin
    // location for proper visualisation.
    if (position != nullptr) {
      auto origin = Json::Value(Json::objectValue);
      origin["position"] = position->to_json();
      if (callee != nullptr) {
        origin["method"] = callee->to_json();
      }
      taint["origin"] = origin;
    }
  } else if (
      !call_kind.is_declaration() &&
      !call_kind.is_propagation_without_trace()) {
    // Never emit calls for declarations and propagations without traces.
    // Emit it for everything else.
    auto call = Json::Value(Json::objectValue);
    if (callee != nullptr) {
      call["resolves_to"] = callee->to_json();
    }
    if (position != nullptr) {
      call["position"] = position->to_json();
    }
    if (!callee_port_.root().is_leaf()) {
      call["port"] = callee_port_.to_json();
    }
    taint["call"] = call;
  }

  if (!locally_inferred_features_.is_bottom() &&
      !locally_inferred_features_.empty()) {
    taint["local_features"] = locally_inferred_features_.to_json();
  }

  if (call_kind.is_origin()) {
    // User features on the origin frame come from the declaration and should be
    // reported in order to show up in the UI. Note that they cannot be stored
    // as locally_inferred_features in CalleePortFrames because they may be
    // defined on different kinds and do not apply to all frames within the
    // propagated CalleePortFrame.
    FeatureMayAlwaysSet local_user_features;
    for (const auto& frame : *this) {
      local_user_features.add_always(frame.user_features());
    }
    if (!local_user_features.is_bottom() && !local_user_features.empty()) {
      taint["local_user_features"] = local_user_features.to_json();
    }
  }

  if (local_positions_.is_value() && !local_positions_.empty()) {
    taint["local_positions"] = local_positions_.to_json();
  }

  return taint;
}

} // namespace marianatrench
