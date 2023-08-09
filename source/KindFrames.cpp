/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/Heuristics.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/KindFrames.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/PathTreeDomain.h>

namespace marianatrench {

KindFrames::KindFrames(std::initializer_list<TaintConfig> configs)
    : kind_(nullptr), frames_(FramesByInterval::bottom()) {
  for (const auto& config : configs) {
    add(config);
  }
}

bool KindFrames::is_bottom() const {
  auto is_bottom = frames_.is_bottom();
  // kind == nullptr iff (is_bottom or is_top)
  // Not strictly required for overall correctness, but is a convenient
  // invariant to maintain for clarity around what each state means.
  // Ideally, this check should include is_top too, but the domain is never set
  // to top.
  mt_assert(is_bottom || kind_ != nullptr);
  mt_assert(!is_bottom || kind_ == nullptr);
  return is_bottom;
}

bool KindFrames::is_top() const {
  // This domain is never set to top, but is_top() checks happen in other
  // operations when this domain is contained within another abstract domain
  // (e.g. PatricaTreeMapAbstractPartition::leq)
  return frames_.is_top();
}

void KindFrames::set_to_bottom() {
  kind_ = nullptr;
  frames_.set_to_bottom();
}

void KindFrames::set_to_top() {
  mt_unreachable();
}

bool KindFrames::leq(const KindFrames& other) const {
  mt_assert(is_bottom() || other.is_bottom() || kind_ == other.kind_);
  return frames_.leq(other.frames_);
}

bool KindFrames::equals(const KindFrames& other) const {
  mt_assert(is_bottom() || other.is_bottom() || kind_ == other.kind_);
  return frames_.equals(other.frames_);
}

void KindFrames::join_with(const KindFrames& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    kind_ = other.kind_;
  }
  mt_assert(other.is_bottom() || kind_ == other.kind_);

  frames_.join_with(other.frames_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void KindFrames::widen_with(const KindFrames& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    kind_ = other.kind_;
  }
  mt_assert(other.is_bottom() || kind_ == other.kind_);

  frames_.widen_with(other.frames_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void KindFrames::meet_with(const KindFrames& other) {
  if (is_bottom()) {
    kind_ = other.kind_;
  }
  mt_assert(other.is_bottom() || kind_ == other.kind_);

  frames_.meet_with(other.frames_);
  if (frames_.is_bottom()) {
    set_to_bottom();
  }
}

void KindFrames::narrow_with(const KindFrames& other) {
  if (is_bottom()) {
    kind_ = other.kind_;
  }
  mt_assert(other.is_bottom() || kind_ == other.kind_);

  frames_.narrow_with(other.frames_);
  if (frames_.is_bottom()) {
    set_to_bottom();
  }
}

KindFrames::KindFrames(const Frame& frame) : KindFrames() {
  if (!frame.is_bottom()) {
    add(frame);
  }
}

void KindFrames::add(const TaintConfig& config) {
  if (is_bottom()) {
    kind_ = config.kind();
  } else {
    mt_assert(kind_ == config.kind());
  }

  frames_.update(CalleeInterval(config), [&config](Frame* frame) {
    frame->join_with(Frame(config));
  });
}

void KindFrames::add(const Frame& frame) {
  if (is_bottom()) {
    kind_ = frame.kind();
  } else {
    mt_assert(kind_ == frame.kind());
  }

  frames_.update(CalleeInterval(frame), [&frame](Frame* original_frame) {
    original_frame->join_with(frame);
  });
}

void KindFrames::difference_with(const KindFrames& other) {
  if (is_bottom()) {
    kind_ = other.kind();
  }
  mt_assert(other.is_bottom() || kind_ == other.kind_);

  FramesByInterval new_frames;
  for (const auto& [key, value] : frames_.bindings()) {
    auto other_value = other.frames_.get(key);
    if (value.leq(other_value)) {
      continue;
    }
    new_frames.set(key, value);
  }

  if (new_frames.is_bottom()) {
    set_to_bottom();
  } else {
    frames_ = std::move(new_frames);
  }
}

void KindFrames::set_origins_if_empty(const MethodSet& origins) {
  map([&origins](Frame frame) {
    if (frame.origins().empty()) {
      frame.set_origins(origins);
    }
    return frame;
  });
}

void KindFrames::set_field_origins_if_empty_with_field_callee(
    const Field* field) {
  map([field](Frame frame) {
    if (frame.field_origins().empty()) {
      frame.set_field_origins(FieldSet{field});
    }
    frame.set_field_callee(field);
    return frame;
  });
}

void KindFrames::append_to_propagation_output_paths(
    Path::Element path_element) {
  map([path_element](Frame frame) {
    frame.append_to_propagation_output_paths(path_element);
    return frame;
  });
}

KindFrames KindFrames::propagate(
    const Method* callee,
    const AccessPath& callee_port,
    const Position* call_position,
    const FeatureMayAlwaysSet& locally_inferred_features,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    std::vector<const Feature*>& via_type_of_features_added) const {
  // TODO(T158171922): Handle intervals. For now, intervals are ignored and
  // all frames with different intervals propagated together.
  if (is_bottom()) {
    return KindFrames::bottom();
  }

  const auto* kind = kind_;
  mt_assert(kind != nullptr);

  int distance = std::numeric_limits<int>::max();
  auto origins = MethodSet::bottom();
  auto field_origins = FieldSet::bottom();
  auto inferred_features = FeatureMayAlwaysSet::bottom();
  auto user_features = FeatureSet::bottom();
  auto output_paths = PathTreeDomain::bottom();
  std::optional<CallInfo> call_info = std::nullopt;

  for (const Frame& frame : *this) {
    if (call_info == std::nullopt) {
      call_info = frame.call_info();
    } else {
      mt_assert(frame.call_info() == call_info);
    }

    if (frame.distance() >= maximum_source_sink_distance) {
      continue;
    }

    origins.join_with(frame.origins());
    field_origins.join_with(frame.field_origins());

    auto local_features = locally_inferred_features;
    if (call_info->is_declaration()) {
      // If propagating a declaration, user features are kept as user features
      // in the propagated frame(s). Inferred features are not expected on
      // declarations.
      mt_assert(
          frame.inferred_features().is_bottom() ||
          frame.inferred_features().empty());
      // User features should be propagated from the declaration frame in order
      // for them to show up at the leaf frame (e.g. in the UI).
      // TODO(T158171922): When class intervals are enabled, add test case to
      // verify that the join occurs.
      user_features.join_with(frame.user_features());
    } else {
      // Otherwise, user features are considered part of the propagated set of
      // (non-locally) inferred features.
      local_features.add(frame.features());
    }
    inferred_features.join_with(local_features);

    // Address clangtidy nullptr dereference warning. `callee` cannot actually
    // be nullptr here in practice.
    mt_assert(callee != nullptr);
    auto via_type_of_features = frame.materialize_via_type_of_ports(
        callee, context.feature_factory, source_register_types);
    for (const auto* feature : via_type_of_features) {
      via_type_of_features_added.push_back(feature);
      inferred_features.add_always(feature);
    }

    auto via_value_features = frame.materialize_via_value_of_ports(
        callee, context.feature_factory, source_constant_arguments);
    for (const auto* feature : via_value_features) {
      inferred_features.add_always(feature);
    }

    // It's important that this check happens after we materialize the ports for
    // the error messages on failure to not get null callables.
    if (call_info->is_declaration()) {
      // If we're propagating a declaration, there are no origins, and we should
      // keep the set as bottom, and set the callee to nullptr explicitly to
      // avoid emitting an invalid frame.
      distance = 0;
      callee = nullptr;
    } else {
      distance = std::min(distance, frame.distance() + 1);
    }

    if (call_info->is_propagation_with_trace()) {
      // Propagate the output paths for PropagationWithTrace frames.
      output_paths.join_with(frame.output_paths());
    }
  }

  // For `TransformKind`, all local_transforms of the callee become
  // global_transforms for the caller.
  if (const auto* transform_kind = kind->as<TransformKind>()) {
    kind = context.kind_factory->transform_kind(
        /* base_kind */ transform_kind->base_kind(),
        /* local_transforms */ nullptr,
        /* global_transforms */
        context.transforms_factory->concat(
            transform_kind->local_transforms(),
            transform_kind->global_transforms()));
  }

  if (kind == nullptr) {
    // Invalid sequence of transforms.
    return KindFrames::bottom();
  }

  if (distance == std::numeric_limits<int>::max()) {
    return KindFrames::bottom();
  }

  mt_assert(distance <= maximum_source_sink_distance);
  mt_assert(call_info != std::nullopt);
  CallInfo propagated_call_info = call_info->propagate();
  if (distance > 0) {
    mt_assert(
        !propagated_call_info.is_declaration() &&
        !propagated_call_info.is_origin());
  } else {
    mt_assert(propagated_call_info.is_origin());
  }

  auto propagated_frame = Frame(
      kind,
      callee_port,
      callee,
      /* field_callee */ nullptr, // Since propagate is only called at method
                                  // callsites and not field accesses
      call_position,
      // TODO(T158171922): Actually compute the interval.
      CalleeInterval(),
      distance,
      std::move(origins),
      std::move(field_origins),
      std::move(inferred_features),
      /* user_features */ user_features,
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* canonical_names */ {},
      propagated_call_info,
      output_paths);

  return KindFrames(
      kind,
      FramesByInterval{
          std::pair(CalleeInterval(propagated_frame), propagated_frame)});
}

KindFrames KindFrames::propagate_crtex_leaf_frames(
    const Method* callee,
    const AccessPath& canonical_callee_port,
    const Position* call_position,
    const FeatureMayAlwaysSet& locally_inferred_features,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types)
    const {
  if (is_bottom()) {
    return KindFrames::bottom();
  }

  KindFrames result;
  std::vector<const Feature*> via_type_of_features_added;
  auto propagated = propagate(
      callee,
      canonical_callee_port,
      call_position,
      locally_inferred_features,
      maximum_source_sink_distance,
      context,
      source_register_types,
      /* source_constant_arguments */ {}, // via-value not supported for crtex
      via_type_of_features_added);

  if (propagated.is_bottom()) {
    return KindFrames::bottom();
  }

  // All frames in `propagated` should have `callee` and `callee_port` set to
  // what was passed as its input arguments. Instantiation of canonical names
  // and ports below assume this.
  for (const auto& frame : *this) {
    auto canonical_names = frame.canonical_names();
    if (!canonical_names.is_value() || canonical_names.elements().empty()) {
      WARNING(
          2,
          "Encountered crtex frame without canonical names. Frame: `{}`",
          frame);
      continue;
    }

    CanonicalNameSetAbstractDomain instantiated_names;
    for (const auto& canonical_name : canonical_names.elements()) {
      auto instantiated_name =
          canonical_name.instantiate(callee, via_type_of_features_added);
      if (!instantiated_name) {
        continue;
      }
      instantiated_names.add(*instantiated_name);
    }

    // All fields should be propagated like other frames, except the crtex
    // fields. Ideally, origins should contain the canonical names as well,
    // but canonical names are strings and cannot be stored in MethodSet.
    // Frame is not propagated if none of the canonical names instantiated
    // successfully.
    if (instantiated_names.is_value() &&
        !instantiated_names.elements().empty()) {
      for (const auto& propagated_frame : propagated) {
        result.add(Frame(
            kind_,
            canonical_callee_port,
            callee,
            propagated_frame.field_callee(),
            propagated_frame.call_position(),
            propagated_frame.callee_interval(),
            /* distance (always leaves for crtex frames) */ 0,
            propagated_frame.origins(),
            propagated_frame.field_origins(),
            propagated_frame.inferred_features(),
            propagated_frame.user_features(),
            propagated_frame.via_type_of_ports(),
            propagated_frame.via_value_of_ports(),
            /* canonical_names */ instantiated_names,
            CallInfo::callsite(),
            /* output_paths */ PathTreeDomain::bottom()));
      }
    }
  }

  return result;
}

void KindFrames::filter_invalid_frames(
    const std::function<bool(const Method*, const AccessPath&, const Kind*)>&
        is_valid) {
  FramesByInterval new_frames;
  for (const auto& frame : *this) {
    if (is_valid(frame.callee(), frame.callee_port(), frame.kind())) {
      new_frames.set(CalleeInterval(frame), frame);
    }
  }

  if (new_frames.is_bottom()) {
    set_to_bottom();
  } else {
    frames_ = std::move(new_frames);
  }
}

bool KindFrames::contains_kind(const Kind* kind) const {
  return kind_ == kind;
}

KindFrames KindFrames::with_kind(const Kind* kind) const {
  KindFrames result;
  for (const auto& frame : *this) {
    auto frame_with_kind = frame.with_kind(kind);
    result.add(frame_with_kind);
  }
  return result;
}

void KindFrames::add_inferred_features(const FeatureMayAlwaysSet& features) {
  map([&features](Frame frame) {
    frame.add_inferred_features(features);
    return frame;
  });
}

std::ostream& operator<<(std::ostream& out, const KindFrames& frames) {
  mt_assert(!frames.frames_.is_top());
  out << "KindFrames(frames=[";
  for (const auto& [interval, frame] : frames.frames_.bindings()) {
    out << "FramesByInterval(interval=" << show(interval) << ", frame=" << frame
        << "),";
  }
  return out << "])";
}

} // namespace marianatrench
