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

  frames_.update(CallClassIntervalContext(config), [&config](Frame* frame) {
    frame->join_with(Frame(config));
  });
}

void KindFrames::add(const Frame& frame) {
  if (is_bottom()) {
    kind_ = frame.kind();
  } else {
    mt_assert(kind_ == frame.kind());
  }

  frames_.update(
      CallClassIntervalContext(frame),
      [&frame](Frame* original_frame) { original_frame->join_with(frame); });
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

namespace {

const Kind* propagate_kind(const Kind* kind, Context& context) {
  mt_assert(kind != nullptr);

  // For `TransformKind`, all local_transforms of the callee become
  // global_transforms for the caller.
  if (const auto* transform_kind = kind->as<TransformKind>()) {
    return context.kind_factory->transform_kind(
        /* base_kind */ transform_kind->base_kind(),
        /* local_transforms */ nullptr,
        /* global_transforms */
        context.transforms_factory->concat(
            transform_kind->local_transforms(),
            transform_kind->global_transforms()));
  }

  return kind;
}

CallClassIntervalContext propagate_interval(
    const Frame& frame,
    const CallClassIntervalContext& class_interval_context,
    const ClassIntervals::Interval& caller_class_interval) {
  const auto& frame_interval = frame.class_interval_context();
  if (frame.call_info().is_declaration()) {
    // The source/sink declaration is the base case. Its propagated frame
    // (caller -> callee with source/sink) should have the properties:
    //
    // 1. Propagated interval is that of the caller's class since the
    //    source/sink call occurs in the context of the caller's class.
    // 2. Although it may not be a this.* call, the propagated interval occurs
    //    in the context of the caller's class => preserves_type_context = true.
    mt_assert(frame_interval.is_default());
    return CallClassIntervalContext(
        caller_class_interval, /* preserves_type_context */ true);
  }

  auto propagated_interval = class_interval_context.callee_interval();
  if (frame_interval.preserves_type_context()) {
    // If the frame representing a (f() -> g()) call preserves the type context,
    // it is either a call to a declared source/sink, or a this.* call. The
    // frame's interval must intersect with the class_interval_context, which is
    // the interval of the receiver in receiver.f()), i.e. the receiver type
    // should be a derived class of the class which f() is defined in.
    propagated_interval = frame_interval.callee_interval().meet(
        class_interval_context.callee_interval());
  }

  return CallClassIntervalContext(
      propagated_interval, class_interval_context.preserves_type_context());
}

// Returns the propagated inferred features.
// User features, if any, are "returned" via the `propagated_user_features`
// argument.
FeatureMayAlwaysSet propagate_features(
    const Frame& frame,
    const FeatureMayAlwaysSet& locally_inferred_features,
    const Method* callee,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    FeatureSet& propagated_user_features,
    std::vector<const Feature*>& via_type_of_features_added) {
  auto propagated_local_features = locally_inferred_features;
  if (frame.call_info().is_declaration()) {
    // If propagating a declaration, user features are kept as user features
    // in the propagated frame(s). Inferred features are not expected on
    // declarations.
    mt_assert(
        frame.inferred_features().is_bottom() ||
        frame.inferred_features().empty());
    // User features should be propagated from the declaration frame in order
    // for them to show up at the leaf frame (e.g. in the UI).
    propagated_user_features = frame.user_features();
  } else {
    // Otherwise, user features are considered part of the propagated set of
    // (non-locally) inferred features.
    propagated_local_features.add(frame.features());
    propagated_user_features = FeatureSet::bottom();
  }

  auto inferred_features = propagated_local_features;

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

  return inferred_features;
}

} // namespace

KindFrames KindFrames::propagate(
    const Method* callee,
    const AccessPath& callee_port,
    const Position* call_position,
    const FeatureMayAlwaysSet& locally_inferred_features,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    std::vector<const Feature*>& via_type_of_features_added,
    const CallClassIntervalContext& class_interval_context,
    const ClassIntervals::Interval& caller_class_interval) const {
  if (is_bottom()) {
    return KindFrames::bottom();
  }

  const auto* kind = propagate_kind(kind_, context);
  mt_assert(kind != nullptr);

  FramesByInterval propagated_frames;
  for (const auto& [_interval, frame] : frames_.bindings()) {
    auto propagated_interval = propagate_interval(
        frame, class_interval_context, caller_class_interval);
    if (propagated_interval.callee_interval().is_bottom()) {
      // Intervals do not intersect. Do not propagate this frame.
      continue;
    }

    int distance = std::numeric_limits<int>::max();
    auto origins = frame.origins();
    auto field_origins = frame.field_origins();
    auto call_info = frame.call_info();
    auto output_paths = PathTreeDomain::bottom();

    if (frame.distance() >= maximum_source_sink_distance) {
      continue;
    }

    auto propagated_user_features = FeatureSet::bottom();
    auto inferred_features = propagate_features(
        frame,
        locally_inferred_features,
        callee,
        context,
        source_register_types,
        source_constant_arguments,
        propagated_user_features,
        via_type_of_features_added);

    const auto* propagated_callee = callee;
    if (call_info.is_declaration()) {
      // If we're propagating a declaration, there are no origins, and we should
      // keep the set as bottom, and set the callee to nullptr explicitly to
      // avoid emitting an invalid frame.
      distance = 0;
      propagated_callee = nullptr;
    } else {
      distance = frame.distance() + 1;
    }

    if (call_info.is_propagation_with_trace()) {
      // Propagate the output paths for PropagationWithTrace frames.
      output_paths.join_with(frame.output_paths());
    }

    mt_assert(distance <= maximum_source_sink_distance);

    CallInfo propagated_call_info = call_info.propagate();
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
        propagated_callee,
        /* field_callee */ nullptr, // Since propagate is only called at method
                                    // callsites and not field accesses
        call_position,
        propagated_interval,
        distance,
        std::move(origins),
        std::move(field_origins),
        std::move(inferred_features),
        propagated_user_features,
        /* via_type_of_ports */ {},
        /* via_value_of_ports */ {},
        /* canonical_names */ {},
        propagated_call_info,
        output_paths,
        /* extra_traces */ {});

    propagated_frames.update(
        propagated_interval, [&propagated_frame](Frame* frame) {
          frame->join_with(propagated_frame);
        });
  }

  if (propagated_frames.is_bottom()) {
    return KindFrames::bottom();
  }

  return KindFrames(kind, propagated_frames);
}

KindFrames KindFrames::propagate_crtex_leaf_frames(
    const Method* callee,
    const AccessPath& canonical_callee_port,
    const Position* call_position,
    const FeatureMayAlwaysSet& locally_inferred_features,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const CallClassIntervalContext& class_interval_context,
    const ClassIntervals::Interval& caller_class_interval) const {
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
      via_type_of_features_added,
      class_interval_context,
      caller_class_interval);

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
            propagated_frame.class_interval_context(),
            /* distance (always leaves for crtex frames) */ 0,
            propagated_frame.origins(),
            propagated_frame.field_origins(),
            propagated_frame.inferred_features(),
            propagated_frame.user_features(),
            propagated_frame.via_type_of_ports(),
            propagated_frame.via_value_of_ports(),
            /* canonical_names */ instantiated_names,
            CallInfo::callsite(),
            /* output_paths */ PathTreeDomain::bottom(),
            /* extra_traces */ propagated_frame.extra_traces()));
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
      new_frames.set(CallClassIntervalContext(frame), frame);
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
