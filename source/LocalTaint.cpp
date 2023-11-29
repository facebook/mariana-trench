/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/ExtraTraceSet.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/Heuristics.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/LocalTaint.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/PathTreeDomain.h>

namespace marianatrench {

LocalTaint::LocalTaint(std::initializer_list<TaintConfig> configs)
    : LocalTaint() {
  for (const auto& config : configs) {
    add(config);
  }
}

LocalTaint::LocalTaint(const Frame& frame) : LocalTaint() {
  add(frame);
}

void LocalTaint::add(const TaintConfig& config) {
  if (is_bottom()) {
    call_info_ = CallInfo(
        config.callee(),
        config.call_kind(),
        AccessPathFactory::singleton().get(config.callee_port()),
        config.call_position());
  } else {
    mt_assert(
        call_info_.callee() == config.callee() &&
        call_info_.call_kind() == config.call_kind() &&
        *call_info_.callee_port() == config.callee_port() &&
        call_info_.call_position() == config.call_position());
  }

  local_positions_.join_with(config.local_positions());
  locally_inferred_features_.join_with(config.locally_inferred_features());
  frames_.update(config.kind(), [&config](const KindFrames& frames) {
    auto frames_copy = frames;
    frames_copy.add(config);
    return frames_copy;
  });
}

bool LocalTaint::leq(const LocalTaint& other) const {
  if (is_bottom()) {
    return true;
  } else if (other.is_bottom()) {
    return false;
  }
  mt_assert(call_info_ == other.call_info_);
  return frames_.leq(other.frames_) &&
      local_positions_.leq(other.local_positions_) &&
      locally_inferred_features_.leq(other.locally_inferred_features_);
}

bool LocalTaint::equals(const LocalTaint& other) const {
  mt_assert(is_bottom() || other.is_bottom() || call_info_ == other.call_info_);
  return frames_.equals(other.frames_) &&
      local_positions_.equals(other.local_positions_) &&
      locally_inferred_features_.equals(other.locally_inferred_features_);
}

void LocalTaint::join_with(const LocalTaint& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    call_info_ = other.call_info_;
  }
  mt_assert(other.is_bottom() || call_info_ == other.call_info_);

  frames_.join_with(other.frames_);
  local_positions_.join_with(other.local_positions_);
  locally_inferred_features_.join_with(other.locally_inferred_features_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void LocalTaint::widen_with(const LocalTaint& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    call_info_ = other.call_info_;
  }
  mt_assert(other.is_bottom() || call_info_ == other.call_info_);

  frames_.widen_with(other.frames_);
  local_positions_.widen_with(other.local_positions_);
  locally_inferred_features_.widen_with(other.locally_inferred_features_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void LocalTaint::meet_with(const LocalTaint& other) {
  if (is_bottom()) {
    call_info_ = other.call_info_;
  }
  mt_assert(other.is_bottom() || call_info_ == other.call_info_);

  frames_.meet_with(other.frames_);
  if (frames_.is_bottom()) {
    set_to_bottom();
  } else {
    local_positions_.meet_with(other.local_positions_);
    locally_inferred_features_.meet_with(other.locally_inferred_features_);
  }
}

void LocalTaint::narrow_with(const LocalTaint& other) {
  if (is_bottom()) {
    call_info_ = other.call_info_;
  }
  mt_assert(other.is_bottom() || call_info_ == other.call_info_);

  frames_.narrow_with(other.frames_);
  if (frames_.is_bottom()) {
    set_to_bottom();
  } else {
    local_positions_.narrow_with(other.local_positions_);
    locally_inferred_features_.narrow_with(other.locally_inferred_features_);
  }
}

void LocalTaint::difference_with(const LocalTaint& other) {
  if (is_bottom()) {
    call_info_ = other.call_info_;
  }
  mt_assert(other.is_bottom() || call_info_ == other.call_info_);

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

void LocalTaint::add_origins_if_declaration(
    const Method* method,
    const AccessPath* port) {
  if (!call_kind().is_declaration()) {
    return;
  }

  transform_frames([method, port](Frame frame) {
    frame.add_origin(method, port);
    return frame;
  });
}

void LocalTaint::add_origins_if_declaration(const Field* field) {
  if (!call_kind().is_declaration()) {
    return;
  }

  transform_frames([field](Frame frame) {
    frame.add_origin(field);
    return frame;
  });
}

void LocalTaint::add_locally_inferred_features(
    const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  locally_inferred_features_.add(features);
}

void LocalTaint::add_local_position(const Position* position) {
  if (call_kind().is_propagation()) {
    return; // Do not add local positions on propagations.
  }

  local_positions_.add(position);
}

void LocalTaint::set_local_positions(LocalPositionSet positions) {
  if (call_kind().is_propagation()) {
    return; // Do not add local positions on propagations.
  }

  local_positions_ = std::move(positions);
}

void LocalTaint::append_to_propagation_output_paths(
    Path::Element path_element) {
  if (!call_kind().is_propagation()) {
    return;
  }

  transform_frames([path_element](Frame frame) {
    frame.append_to_propagation_output_paths(path_element);
    return frame;
  });
}

namespace {

template <typename Function> // Frame(Frame)
sparta::PatriciaTreeMapAbstractPartition<const Kind*, KindFrames>
map_frames_by_kind(
    sparta::PatriciaTreeMapAbstractPartition<const Kind*, KindFrames> frames,
    Function&& f) {
  frames.transform([f = std::forward<Function>(f)](KindFrames kind_frames) {
    kind_frames.transform(f);
    return kind_frames;
  });
  return frames;
}

} // namespace

LocalTaint LocalTaint::propagate(
    const Method* MT_NULLABLE callee,
    const AccessPath& callee_port,
    const Position* call_position,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    const CallClassIntervalContext& class_interval_context,
    const ClassIntervals::Interval& caller_class_interval) const {
  if (is_bottom()) {
    return LocalTaint::bottom();
  }

  mt_assert(!call_kind().is_propagation_without_trace());
  auto propagated_call_info =
      call_info_.propagate(callee, callee_port, call_position, context);

  FramesByKind propagated_frames_by_kind;
  for (const auto& [kind, frames] : frames_.bindings()) {
    auto propagated = frames.propagate(
        callee,
        propagated_call_info,
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
    return LocalTaint::bottom();
  }

  return LocalTaint(
      propagated_call_info,
      propagated_frames_by_kind,
      /* local_positions */ {},
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom());
}

LocalTaint LocalTaint::update_with_propagation_trace(
    const CallInfo& propagation_call_info,
    const Frame& propagation_frame) const {
  if (is_bottom()) {
    return LocalTaint::bottom();
  }

  const auto& callee_call_kind = call_kind();
  if (!callee_call_kind.is_propagation_without_trace()) {
    // The propagation taint tree tracks the final transform hop as the
    // "callee" so we do not need to "propagate" these calls.
    // All these (prior) transform hops are tracked as ExtraTrace hop
    // frames to create a subtrace.
    LocalTaint result = *this;
    result.transform_frames(
        [&propagation_call_info, &propagation_frame](Frame frame) {
          auto call_kind = propagation_call_info.call_kind();
          if (call_kind.is_propagation_without_trace()) {
            // These should be added as the next hop of the trace.
            return frame;
          }
          frame.add_extra_trace(ExtraTrace(
              propagation_frame.kind(),
              propagation_call_info.callee(),
              propagation_call_info.call_position(),
              propagation_call_info.callee_port(),
              call_kind));
          return frame;
        });

    return result;
  }

  mt_assert(callee_call_kind.is_propagation_without_trace());

  FramesByKind frames =
      map_frames_by_kind(frames_, [&propagation_frame](const Frame& frame) {
        return frame.update_with_propagation_trace(propagation_frame);
      });

  return LocalTaint(
      propagation_call_info,
      std::move(frames),
      local_positions_,
      locally_inferred_features_);
}

LocalTaint LocalTaint::attach_position(const Position* call_position) const {
  if (is_bottom()) {
    return LocalTaint::bottom();
  }

  // Only propagate leaves.
  if (callee() != nullptr) {
    return LocalTaint::bottom();
  }

  // Since attach_position is used (only) for parameter_sinks and return
  // sources which may be included in an issue as a leaf, we need to make
  // sure that those leaf frames in issues contain the user_features as being
  // locally inferred.
  FeatureSet user_features;
  FramesByKind frames = map_frames_by_kind(
      frames_,
      [&locally_inferred_features = locally_inferred_features_,
       &user_features](const Frame& frame) {
        user_features.join_with(
            frame.user_features()); // Collect all user features.
        auto inferred_features = frame.features();
        inferred_features.add(locally_inferred_features);
        // Consider using a propagate() call here.
        return Frame(
            frame.kind(),
            // TODO(T158171922): Re-visit what the appropriate interval
            // should be when implementing class intervals.
            frame.class_interval_context(),
            /* distance */ 0,
            frame.origins(),
            inferred_features,
            /* user_features */ FeatureSet::bottom(),
            /* via_type_of_ports */ {},
            /* via_value_of_ports */ {},
            frame.canonical_names(),
            /* output_paths */ {},
            frame.extra_traces());
      });

  auto locally_inferred_features = user_features.is_bottom()
      ? FeatureMayAlwaysSet::bottom()
      : FeatureMayAlwaysSet::make_always(user_features);
  return LocalTaint(
      CallInfo(
          /* callee */ nullptr,
          /* call_kind */ CallKind::origin(),
          callee_port(),
          call_position),
      std::move(frames),
      local_positions_,
      std::move(locally_inferred_features));
}

LocalTaint LocalTaint::apply_transform(
    const KindFactory& kind_factory,
    const TransformsFactory& transforms,
    const UsedKinds& used_kinds,
    const TransformList* local_transforms) const {
  FramesByKind new_frames;
  this->visit_frames(
      [&new_frames, &kind_factory, &transforms, &used_kinds, local_transforms](
          const CallInfo&, const Frame& frame) {
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
      });

  if (new_frames.is_bottom()) {
    return LocalTaint::bottom();
  }

  return LocalTaint(
      call_info_,
      std::move(new_frames),
      local_positions_,
      locally_inferred_features_);
}

void LocalTaint::update_maximum_collapse_depth(CollapseDepth collapse_depth) {
  if (!call_kind().is_propagation()) {
    return;
  }

  transform_frames([collapse_depth](Frame frame) {
    frame.update_maximum_collapse_depth(collapse_depth);
    return frame;
  });
}

std::vector<LocalTaint> LocalTaint::update_non_declaration_positions(
    const std::function<const Position*(
        const Method*,
        const AccessPath* MT_NULLABLE,
        const Position* MT_NULLABLE)>& map_call_position,
    const std::function<LocalPositionSet(const LocalPositionSet&)>&
        map_local_positions) const {
  if (is_bottom() || call_kind().is_declaration()) {
    // Nothing to update.
    return std::vector{*this};
  }

  auto new_local_positions = map_local_positions(local_positions_);

  if (call_kind().is_origin()) {
    // There can be mulitple callee(s) for origins. These are stored in
    // Frame::origins.
    return update_origin_positions(map_call_position, new_local_positions);
  }

  const auto* callee = this->callee();
  // This should have call kind == CallSite, callee should not be nullptr.
  mt_assert(callee != nullptr);

  const auto* callee_port = this->callee_port();
  const auto* call_position = this->call_position();

  const auto* MT_NULLABLE new_call_position =
      map_call_position(callee, callee_port, call_position);
  auto new_call_info =
      CallInfo(callee, call_kind(), callee_port, new_call_position);

  return {LocalTaint(
      new_call_info, frames_, new_local_positions, locally_inferred_features_)};
}

std::vector<LocalTaint> LocalTaint::update_origin_positions(
    const std::function<const Position*(
        const Method*,
        const AccessPath* MT_NULLABLE,
        const Position* MT_NULLABLE)>& map_call_position,
    const LocalPositionSet& new_local_positions) const {
  mt_assert(this->call_kind().is_origin());
  std::vector<LocalTaint> results;

  const auto* callee = this->callee();
  const auto call_kind = this->call_kind();
  const auto* callee_port = this->callee_port();
  const auto* call_position = this->call_position();

  this->visit_frames([this,
                      &map_call_position,
                      &new_local_positions,
                      &results,
                      &call_kind,
                      callee,
                      callee_port,
                      call_position](const CallInfo&, const Frame& frame) {
    OriginSet non_method_origins{};
    for (const auto* origin : frame.origins()) {
      const auto* method_origin = origin->as<MethodOrigin>();
      if (method_origin == nullptr) {
        // Only method origins have callee information
        non_method_origins.add(origin);
        continue;
      }

      const auto* MT_NULLABLE new_call_position = map_call_position(
          method_origin->method(), callee_port, call_position);
      auto new_call_info =
          CallInfo(callee, call_kind, callee_port, new_call_position);
      results.emplace_back(LocalTaint(
          new_call_info,
          FramesByKind({std::pair(
              frame.kind(),
              KindFrames(frame.with_origins(OriginSet{method_origin})))}),
          new_local_positions,
          locally_inferred_features_));
    }

    // Non-method origins will not have positions updated but their
    // information should be retained.
    if (!non_method_origins.empty()) {
      results.emplace_back(LocalTaint(
          call_info_,
          FramesByKind({std::pair(
              frame.kind(),
              KindFrames(frame.with_origins(non_method_origins)))}),
          new_local_positions,
          locally_inferred_features_));
    }
  });

  // This can only happen if there are no origins to begin with, which points
  // to a problem with populating them correctly during model generation.
  mt_assert(!results.empty());
  return results;
}

void LocalTaint::filter_invalid_frames(
    const std::function<
        bool(const Method* MT_NULLABLE, const AccessPath&, const Kind*)>&
        is_valid) {
  if (is_bottom()) {
    return;
  }

  frames_.transform([&is_valid,
                     &call_info = call_info_](KindFrames kind_frames) {
    kind_frames.filter_invalid_frames([&is_valid, call_info](const Kind* kind) {
      return is_valid(call_info.callee(), *call_info.callee_port(), kind);
    });
    return kind_frames;
  });

  if (frames_.is_bottom()) {
    set_to_bottom();
  }
}

bool LocalTaint::contains_kind(const Kind* kind) const {
  return !frames_.get(kind).is_bottom();
}

FeatureMayAlwaysSet LocalTaint::features_joined() const {
  auto features = FeatureMayAlwaysSet::bottom();
  this->visit_frames([&features, this](const CallInfo&, const Frame& frame) {
    auto combined_features = frame.features();
    combined_features.add(locally_inferred_features_);
    features.join_with(combined_features);
  });
  return features;
}

std::ostream& operator<<(std::ostream& out, const LocalTaint& frames) {
  mt_assert(!frames.frames_.is_top());
  out << "LocalTaint(call_info=" << frames.call_info_;

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

void LocalTaint::add(const Frame& frame) {
  if (frame.is_bottom()) {
    return;
  }

  frames_.update(frame.kind(), [&frame](const KindFrames& old_frames) {
    auto frames_copy = old_frames;
    frames_copy.add(frame);
    return frames_copy;
  });
}

Json::Value LocalTaint::to_json(ExportOriginsMode export_origins_mode) const {
  auto taint = call_info_.to_json();
  mt_assert(taint.isObject() && !taint.isNull());

  auto kinds = Json::Value(Json::arrayValue);
  this->visit_frames([&kinds, export_origins_mode](
                         const CallInfo& call_info, const Frame& frame) {
    kinds.append(frame.to_json(call_info, export_origins_mode));
  });
  taint["kinds"] = kinds;

  if (!locally_inferred_features_.is_bottom() &&
      !locally_inferred_features_.empty()) {
    taint["local_features"] = locally_inferred_features_.to_json();
  }

  if (call_kind().is_origin()) {
    // User features on the origin frame come from the declaration and should
    // be reported in order to show up in the UI. Note that they cannot be
    // stored as locally_inferred_features in LocalTaint because they may be
    // defined on different kinds and do not apply to all frames within the
    // propagated CalleePortFrame.
    FeatureMayAlwaysSet local_user_features;
    this->visit_frames(
        [&local_user_features](const CallInfo&, const Frame& frame) {
          local_user_features.add_always(frame.user_features());
        });
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
