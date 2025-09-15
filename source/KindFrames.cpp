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
#include <mariana-trench/KindFrames.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/PathTreeDomain.h>
#include <mariana-trench/TransformOperations.h>
#include <mariana-trench/UsedKinds.h>

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

  frames_.difference_like_operation(
      other.frames_, [](Frame* left, const Frame& right) -> void {
        if (left->leq(right)) {
          left->set_to_bottom();
        }
      });

  if (frames_.is_bottom()) {
    set_to_bottom();
  }
}

std::size_t KindFrames::num_frames() const {
  std::size_t count = 0;
  this->visit([&count](const Frame&) { ++count; });
  return count;
}

void KindFrames::append_to_propagation_output_paths(
    Path::Element path_element) {
  frames_.transform([path_element](Frame* frame) -> void {
    frame->append_to_propagation_output_paths(path_element);
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
    const CallInfo& propagated_call_info,
    const CallClassIntervalContext& class_interval_context,
    const ClassIntervals::Interval& caller_class_interval) {
  const auto& frame_interval = frame.class_interval_context();
  if (propagated_call_info.call_kind().is_origin()) {
    // The source/sink declaration is the base case. Its propagated (origin)
    // frame (caller -> callee with source/sink) should have the properties:
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
    const CallInfo& propagated_call_info,
    const FeatureMayAlwaysSet& locally_inferred_features,
    const Method* MT_NULLABLE callee,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    FeatureSet& propagated_user_features,
    std::vector<const Feature*>& via_type_of_features_added) {
  auto propagated_local_features = locally_inferred_features;
  if (propagated_call_info.call_kind().is_origin()) {
    // Inferred features are not expected on an unpropagated declaration frame.
    mt_assert(
        frame.inferred_features().is_bottom() ||
        frame.inferred_features().empty());
    // User features should be propagated from the declaration frame in order
    // for them to show up at the origin (leaf) frame (e.g. in the UI).
    propagated_user_features = frame.user_features();
  } else {
    // Otherwise, user features are considered part of the propagated set of
    // (non-locally) inferred features.
    propagated_local_features.add(frame.features());
    propagated_user_features = FeatureSet::bottom();
  }

  // If the callee is nullptr (e.g. "call" to a field), there are no via-*
  // ports to materialize
  if (callee == nullptr) {
    return propagated_local_features;
  }

  // The via-type/value-of features are also treated as user features.
  // They need to show up on the frame in which they are materialized.
  auto via_type_of_features = frame.materialize_via_type_of_ports(
      callee, context.feature_factory, source_register_types);
  for (const auto* feature : via_type_of_features) {
    via_type_of_features_added.push_back(feature);
    propagated_user_features.add(feature);
  }

  auto via_value_features = frame.materialize_via_value_of_ports(
      callee, context.feature_factory, source_constant_arguments);
  for (const auto* feature : via_value_features) {
    propagated_user_features.add(feature);
  }

  return propagated_local_features;
}

CanonicalNameSetAbstractDomain propagate_canonical_names(
    const Frame& frame,
    const Method* MT_NULLABLE callee,
    const std::vector<const Feature*>& via_type_of_features_added) {
  auto canonical_names = frame.canonical_names();
  if (!canonical_names.is_value() || canonical_names.elements().empty()) {
    // Non-crtex frame
    return CanonicalNameSetAbstractDomain{};
  }

  // Callee should not be nullptr for CRTEX frames because models with
  // canonical names are always defined on methods (as opposed to fields).
  mt_assert(callee != nullptr);

  auto first_name = canonical_names.elements().begin();
  if (first_name->instantiated_value().has_value()) {
    // The canonical names are either all instantiated values, or all
    // templated values that need to be instantiated. Instantiated values do
    // not need to be propagated.
    return CanonicalNameSetAbstractDomain{};
  }

  CanonicalNameSetAbstractDomain instantiated_names{};
  for (const auto& canonical_name : canonical_names.elements()) {
    auto instantiated_name =
        canonical_name.instantiate(callee, via_type_of_features_added);
    if (!instantiated_name) {
      continue;
    }
    instantiated_names.add(*instantiated_name);
  }

  return instantiated_names;
}

} // namespace

KindFrames KindFrames::propagate(
    const Method* MT_NULLABLE callee,
    const CallInfo& propagated_call_info,
    const FeatureMayAlwaysSet& locally_inferred_features,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    const CallClassIntervalContext& class_interval_context,
    const ClassIntervals::Interval& caller_class_interval) const {
  if (is_bottom()) {
    return KindFrames::bottom();
  }

  const auto* kind = propagate_kind(kind_, context);
  mt_assert(kind != nullptr);

  FramesByInterval propagated_frames;
  for (const auto& [_interval, frame] : frames_.bindings()) {
    if (frame.distance() >= maximum_source_sink_distance) {
      continue;
    }

    auto propagated_interval = propagate_interval(
        frame,
        propagated_call_info,
        class_interval_context,
        caller_class_interval);
    if (propagated_interval.callee_interval().is_bottom()) {
      // Intervals do not intersect. Do not propagate this frame.
      continue;
    }

    std::vector<const Feature*> via_type_of_features_added;
    auto propagated_user_features = FeatureSet::bottom();
    auto propagated_inferred_features = propagate_features(
        frame,
        propagated_call_info,
        locally_inferred_features,
        callee,
        context,
        source_register_types,
        source_constant_arguments,
        propagated_user_features,
        via_type_of_features_added);

    // Canonical names can only be instantiated after propagate_features because
    // via_type_of_features_added should be populated based on the features
    // added there.
    auto propagated_canonical_names =
        propagate_canonical_names(frame, callee, via_type_of_features_added);
    // We do not use bottom() for canonical names, only empty().
    mt_assert(propagated_canonical_names.is_value());

    // Propagate instantiated canonical names into origins.
    auto propagated_origins = frame.origins();
    propagated_origins.join_with(CanonicalName::propagate(
        propagated_canonical_names, *propagated_call_info.callee_port()));

    int propagated_distance = frame.distance() + 1;
    auto propagated_call_kind = propagated_call_info.call_kind();
    if (propagated_call_kind.is_origin()) {
      // Origins are the "leaf" of a trace and start at distance 0.
      propagated_distance = 0;
    }
    mt_assert(propagated_distance <= maximum_source_sink_distance);

    auto propagated_output_paths = PathTreeDomain::bottom();
    if (propagated_call_kind.is_propagation_with_trace()) {
      // Propagate the output paths for PropagationWithTrace frames.
      propagated_output_paths.join_with(frame.output_paths());
    }

    if (propagated_distance > 0) {
      mt_assert(
          !propagated_call_kind.is_declaration() &&
          !propagated_call_kind.is_origin());
    } else {
      mt_assert(propagated_call_kind.is_origin());
    }

    auto propagated_frame = Frame(
        kind,
        propagated_interval,
        propagated_distance,
        propagated_origins,
        std::move(propagated_inferred_features),
        propagated_user_features,
        /* via_type_of_ports */ {},
        /* via_value_of_ports */ {},
        propagated_canonical_names,
        propagated_output_paths,
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

KindFrames KindFrames::add_sanitize_transform(
    const Sanitizer& sanitizer,
    const KindFactory& kind_factory,
    const TransformsFactory& transforms_factory) const {
  TransformList new_transforms{
      std::vector{sanitizer.to_transform(transforms_factory)}};
  const TransformList* global_transforms = nullptr;
  const auto* base_kind = kind_;

  mt_assert(base_kind != nullptr);

  // Check and see if we can drop some taints here
  // We use transforms::TransformDirection::Backward because this is always
  // called right after backward taint transfer
  if (new_transforms.sanitizes<TransformList::ApplicationDirection::Backward>(
          base_kind, transforms::TransformDirection::Backward)) {
    return KindFrames::bottom();
  }

  // Need a special case for TransformKind, since we need to append the
  // local_transforms to the existing local transforms.
  if (const auto* transform_kind = base_kind->as<TransformKind>()) {
    const auto* existing_transforms = transform_kind->local_transforms();

    //  TransformList::concat requires non-null transform lists.
    if (existing_transforms != nullptr) {
      new_transforms =
          TransformList::concat(&new_transforms, existing_transforms);
    }

    global_transforms = transform_kind->global_transforms();
    base_kind = transform_kind->base_kind();
    new_transforms =
        TransformList::canonicalize(&new_transforms, transforms_factory);
  }

  // Finally put the new transform list into the factory
  const auto* local_transforms = transforms_factory.create(new_transforms);
  mt_assert(local_transforms != nullptr);

  const auto* new_kind = kind_factory.transform_kind(
      base_kind, local_transforms, global_transforms);

  return KindFrames::with_kind(new_kind);
}

KindFrames KindFrames::apply_transform(
    const KindFactory& kind_factory,
    const TransformsFactory& transforms_factory,
    const UsedKinds& used_kinds,
    const TransformList* local_transforms,
    transforms::TransformDirection direction) const {
  const Kind* base_kind = kind_;
  const TransformList* global_transforms = nullptr;

  // See if we can drop some taint here
  if (local_transforms
          ->sanitizes<TransformList::ApplicationDirection::Backward>(
              kind_, direction)) {
    return KindFrames::bottom();
  }

  if (const auto* transform_kind = kind_->as<TransformKind>()) {
    auto existing_local_transforms = transform_kind->local_transforms();
    global_transforms = transform_kind->global_transforms();
    base_kind = transform_kind->base_kind();
    TransformList temp_local_transforms = *local_transforms;

    if (!base_kind->is<PropagationKind>()) {
      temp_local_transforms = TransformList::discard_unmatched_sanitizers(
          &temp_local_transforms, transforms_factory, direction);
    }

    if (temp_local_transforms.size() != 0 && global_transforms != nullptr &&
        (existing_local_transforms == nullptr ||
         !existing_local_transforms->has_non_sanitize_transform())) {
      temp_local_transforms = TransformList::filter_global_sanitizers(
          &temp_local_transforms, global_transforms, transforms_factory);
    }

    // Append existing local_transforms.
    if (existing_local_transforms != nullptr) {
      temp_local_transforms = TransformList::concat(
          &temp_local_transforms, existing_local_transforms);
    }

    // Canonicalize the and create the local transform list from factory if the
    // list is not empty
    local_transforms = temp_local_transforms.size() != 0
        ? transforms_factory.canonicalize(&temp_local_transforms)
        : nullptr;

  } else if (kind_->is<PropagationKind>()) {
    // If the current kind is PropagationKind, set the transform as a global
    // transform. This is done to track the next hops for propagation with
    // trace.
    global_transforms = local_transforms;
    local_transforms = nullptr;
  } else {
    TransformList temp_local_transforms =
        TransformList::discard_unmatched_sanitizers(
            local_transforms, transforms_factory, direction);
    // No need to apply anything if all transforms are sanitizers and are
    // discarded
    if (temp_local_transforms.size() == 0) {
      return *this;
    }

    local_transforms = transforms_factory.canonicalize(&temp_local_transforms);
  }

  mt_assert(base_kind != nullptr);
  const auto* new_kind = kind_factory.transform_kind(
      base_kind, local_transforms, global_transforms);

  if (!used_kinds.should_keep(new_kind)) {
    return KindFrames::bottom();
  }

  return this->with_kind(new_kind);
}

void KindFrames::filter_invalid_frames(
    const std::function<bool(const Kind*)>& is_valid) {
  frames_.transform([&is_valid](Frame* frame) -> void {
    if (!is_valid(frame->kind())) {
      frame->set_to_bottom();
    }
  });
  if (frames_.is_bottom()) {
    set_to_bottom();
  }
}

bool KindFrames::contains_kind(const Kind* kind) const {
  return kind_ == kind;
}

KindFrames KindFrames::with_kind(const Kind* kind) const {
  KindFrames result;
  this->visit([&result, kind](const Frame& frame) {
    result.add(frame.with_kind(kind));
  });
  return result;
}

void KindFrames::add_inferred_features(const FeatureMayAlwaysSet& features) {
  frames_.transform([&features](Frame* frame) -> void {
    frame->add_inferred_features(features);
  });
}

void KindFrames::collapse_class_intervals() {
  FramesByInterval new_frames;
  CallClassIntervalContext default_class_interval;

  // Join all the frames and set class interval to default (top).
  Frame new_frame;
  for (const auto& [_interval, frame] : frames_.bindings()) {
    new_frame.join_with(frame.with_interval(default_class_interval));
  }

  new_frames.set(default_class_interval, new_frame);
  frames_ = std::move(new_frames);
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
