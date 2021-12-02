/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/Features.h>
#include <mariana-trench/FrameSet.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

FrameSet::FrameSet(std::initializer_list<Frame> frames) : kind_(nullptr) {
  for (const auto& frame : frames) {
    add(frame);
  }
}

void FrameSet::add(const Frame& frame) {
  if (kind_ == nullptr) {
    kind_ = frame.kind();
  } else {
    mt_assert(kind_ == frame.kind());
  }

  map_.update(frame.callee(), [&](const CallPositionToSetMap& position_map) {
    auto position_map_copy = position_map;
    position_map_copy.update(frame.call_position(), [&](const Set& set) {
      auto set_copy = set;
      set_copy.add(frame);
      return set_copy;
    });
    return position_map_copy;
  });
}

bool FrameSet::leq(const FrameSet& other) const {
  return map_.leq(other.map_);
}

bool FrameSet::equals(const FrameSet& other) const {
  return map_.equals(other.map_);
}

void FrameSet::join_with(const FrameSet& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (kind_ == nullptr) {
    kind_ = other.kind_;
  }

  map_.join_with(other.map_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void FrameSet::widen_with(const FrameSet& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (kind_ == nullptr) {
    kind_ = other.kind_;
  }

  map_.widen_with(other.map_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void FrameSet::meet_with(const FrameSet& other) {
  if (kind_ == nullptr) {
    kind_ = other.kind_;
  }

  map_.meet_with(other.map_);
}

void FrameSet::narrow_with(const FrameSet& other) {
  if (kind_ == nullptr) {
    kind_ = other.kind_;
  }

  map_.narrow_with(other.map_);
}

void FrameSet::difference_with(const FrameSet& other) {
  if (kind_ == nullptr) {
    kind_ = other.kind_;
  }

  map_.difference_like_operation(
      other.map_,
      [](const CallPositionToSetMap& position_map_left,
         const CallPositionToSetMap& position_map_right) {
        auto position_map_copy = position_map_left;
        position_map_copy.difference_like_operation(
            position_map_right, [](const Set& set_left, const Set& set_right) {
              auto set_copy = set_left;
              set_copy.difference_with(set_right);
              return set_copy;
            });
        return position_map_copy;
      });
}

void FrameSet::map(const std::function<void(Frame&)>& f) {
  map_.map([&](const CallPositionToSetMap& position_map) {
    auto position_map_copy = position_map;
    position_map_copy.map([&](const Set& set) {
      auto set_copy = set;
      set_copy.map(f);
      return set_copy;
    });
    return position_map_copy;
  });
}

void FrameSet::filter(const std::function<bool(const Frame&)>& predicate) {
  map_.map([&](const CallPositionToSetMap& position_map) {
    auto position_map_copy = position_map;
    position_map_copy.map([&](const Set& set) {
      auto set_copy = set;
      set_copy.filter(predicate);
      return set_copy;
    });
    return position_map_copy;
  });
}

void FrameSet::add_inferred_features(const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map([&features](Frame& frame) { frame.add_inferred_features(features); });
}

LocalPositionSet FrameSet::local_positions() const {
  auto result = LocalPositionSet::bottom();
  for (const auto& [_, position_map] : map_.bindings()) {
    for (const auto& [_, set] : position_map.bindings()) {
      for (const auto& frame : set) {
        result.join_with(frame.local_positions());
      }
    }
  }
  return result;
}

void FrameSet::add_local_position(const Position* position) {
  map([position](Frame& frame) { frame.add_local_position(position); });
}

void FrameSet::set_local_positions(const LocalPositionSet& positions) {
  map([&positions](Frame& frame) { frame.set_local_positions(positions); });
}

void FrameSet::add_inferred_features_and_local_position(
    const FeatureMayAlwaysSet& features,
    const Position* MT_NULLABLE position) {
  if (features.empty() && position == nullptr) {
    return;
  }

  map([&features, position](Frame& frame) {
    if (!features.empty()) {
      frame.add_inferred_features(features);
    }
    if (position != nullptr) {
      frame.add_local_position(position);
    }
  });
}

FeatureMayAlwaysSet FrameSet::features_joined() const {
  auto features = FeatureMayAlwaysSet::bottom();
  for (const auto& frame : *this) {
    features.join_with(frame.features());
  }
  return features;
}

FrameSet FrameSet::propagate(
    const Method* /*caller*/,
    const Method* callee,
    const AccessPath& callee_port,
    const Position* call_position,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments)
    const {
  if (is_bottom()) {
    return FrameSet::bottom();
  }

  auto partitioned = partition_map<bool>(
      [](const Frame& frame) { return frame.is_crtex_producer_declaration(); });

  auto frames = propagate_crtex_frames(
      callee,
      callee_port,
      call_position,
      maximum_source_sink_distance,
      context,
      source_register_types,
      partitioned[true]);

  // Non-CRTEX frames can be joined into the same callee
  std::vector<const Feature*> via_type_of_features_added;
  auto non_crtex_frame = propagate_frames(
      callee,
      callee_port,
      call_position,
      maximum_source_sink_distance,
      context,
      source_register_types,
      source_constant_arguments,
      partitioned[false],
      via_type_of_features_added);
  if (!non_crtex_frame.is_bottom()) {
    frames.add(non_crtex_frame);
  }

  return frames;
}

FrameSet FrameSet::attach_position(const Position* position) const {
  FrameSet leaves;

  for (const auto& [_, position_map] : map_.bindings()) {
    for (const auto& [_, set] : position_map.bindings()) {
      for (const auto& frame : set) {
        if (!frame.is_leaf()) {
          continue;
        }

        // Canonical names should theoretically be instantiated here the way
        // they are instantiated in `propagate`, but there is currently no
        // scenario that requires this. If a templated name does get configured,
        // the name will be instantiated when this frame gets propagated.
        leaves.add(Frame(
            frame.kind(),
            frame.callee_port(),
            /* callee */ nullptr,
            /* field_callee */ nullptr,
            /* call_position */ position,
            /* distance */ 0,
            frame.origins(),
            frame.field_origins(),
            frame.features(),
            /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
            /* user_features */ FeatureSet::bottom(),
            /* via_type_of_ports */ {},
            /* via_value_of_ports */ {},
            frame.local_positions(),
            frame.canonical_names()));
      }
    }
  }

  return leaves;
}

FrameSet FrameSet::with_kind(const Kind* kind) const {
  FrameSet frames_copy(*this);
  frames_copy.kind_ = kind;
  frames_copy.map_.map([kind](const CallPositionToSetMap& position_map) {
    auto position_map_copy = position_map;
    position_map_copy.map([kind](const Set& set) {
      Set new_set;
      for (const auto& frame : set) {
        new_set.add(frame.with_kind(kind));
      }
      return new_set;
    });
    return position_map_copy;
  });
  return frames_copy;
}

template <class T>
std::unordered_map<T, std::vector<std::reference_wrapper<const Frame>>>
FrameSet::partition_map(const std::function<T(const Frame&)>& map) const {
  std::unordered_map<T, std::vector<std::reference_wrapper<const Frame>>>
      result;
  for (const auto& [_, position_map] : map_.bindings()) {
    for (const auto& [_, set] : position_map.bindings()) {
      for (const auto& frame : set) {
        auto value = map(frame);
        result[value].push_back(std::cref(frame));
      }
    }
  }

  return result;
}

FrameSet FrameSet::from_json(const Json::Value& value, Context& context) {
  FrameSet frames;
  for (const auto& frame_value : JsonValidation::null_or_array(value)) {
    frames.add(Frame::from_json(frame_value, context));
  }
  return frames;
}

Json::Value FrameSet::to_json() const {
  mt_assert(!is_top());
  auto frames = Json::Value(Json::arrayValue);
  for (const auto& [_, position_map] : map_.bindings()) {
    for (const auto& [_, set] : position_map.bindings()) {
      for (const auto& frame : set) {
        frames.append(frame.to_json());
      }
    }
  }
  return frames;
}

std::ostream& operator<<(std::ostream& out, const FrameSet& frames) {
  if (frames.is_top()) {
    return out << "T";
  } else {
    out << "{";
    for (auto iterator = frames.begin(), end = frames.end(); iterator != end;) {
      out << *iterator;
      iterator++;
      if (iterator != end) {
        out << ", ";
      }
    }
    return out << "}";
  }
}

namespace {

void materialize_via_type_of_ports(
    const Method* callee,
    Context& context,
    const Frame& frame,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    std::vector<const Feature*>& via_type_of_features_added,
    FeatureMayAlwaysSet& inferred_features) {
  if (!frame.via_type_of_ports().is_value() ||
      frame.via_type_of_ports().elements().empty()) {
    return;
  }

  // Materialize via_type_of_ports into features and add them to the inferred
  // features
  for (const auto& port : frame.via_type_of_ports().elements()) {
    if (!port.is_argument() ||
        port.parameter_position() >= source_register_types.size()) {
      ERROR(
          1,
          "Invalid port {} provided for via_type_of ports of method {}.{}",
          port,
          callee->get_class()->str(),
          callee->get_name());
      continue;
    }
    const auto* feature = context.features->get_via_type_of_feature(
        source_register_types[port.parameter_position()]);
    via_type_of_features_added.push_back(feature);
    inferred_features.add_always(feature);
  }
}

void materialize_via_value_of_ports(
    const Method* callee,
    Context& context,
    const Frame& frame,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    FeatureMayAlwaysSet& inferred_features) {
  if (!frame.via_value_of_ports().is_value() ||
      frame.via_value_of_ports().elements().empty()) {
    return;
  }

  // Materialize via_value_of_ports into features and add them to the inferred
  // features
  for (const auto& port : frame.via_value_of_ports().elements()) {
    if (!port.is_argument() ||
        port.parameter_position() >= source_constant_arguments.size()) {
      ERROR(
          1,
          "Invalid port {} provided for via_value_of ports of method {}.{}",
          port,
          callee->get_class()->str(),
          callee->get_name());
      continue;
    }
    const auto* feature = context.features->get_via_value_of_feature(
        source_constant_arguments[port.parameter_position()]);
    inferred_features.add_always(feature);
  }
}

} // namespace

Frame FrameSet::propagate_frames(
    const Method* callee,
    const AccessPath& callee_port,
    const Position* call_position,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    std::vector<std::reference_wrapper<const Frame>> frames,
    std::vector<const Feature*>& via_type_of_features_added) const {
  int distance = std::numeric_limits<int>::max();
  auto origins = MethodSet::bottom();
  auto field_origins = FieldSet::bottom();
  auto inferred_features = FeatureMayAlwaysSet::bottom();

  for (const Frame& frame : frames) {
    if (frame.distance() >= maximum_source_sink_distance) {
      continue;
    }

    distance = std::min(distance, frame.distance() + 1);
    origins.join_with(frame.origins());
    field_origins.join_with(frame.field_origins());

    // Note: This merges user features with existing inferred features.
    inferred_features.join_with(frame.features());

    materialize_via_type_of_ports(
        callee,
        context,
        frame,
        source_register_types,
        via_type_of_features_added,
        inferred_features);

    materialize_via_value_of_ports(
        callee, context, frame, source_constant_arguments, inferred_features);
  }

  if (distance == std::numeric_limits<int>::max()) {
    return Frame::bottom();
  }

  return Frame(
      kind_,
      callee_port,
      callee,
      /* field_callee */ nullptr, // Since propagate is only called at method
                                  // callsites and not field accesses
      call_position,
      distance,
      std::move(origins),
      std::move(field_origins),
      std::move(inferred_features),
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user_features */ FeatureSet::bottom(),
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {});
}

FrameSet FrameSet::propagate_crtex_frames(
    const Method* callee,
    const AccessPath& callee_port,
    const Position* call_position,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    std::vector<std::reference_wrapper<const Frame>> frames) const {
  FrameSet result;

  for (const Frame& frame : frames) {
    std::vector<const Feature*> via_type_of_features_added;
    auto propagated = propagate_frames(
        callee,
        callee_port,
        call_position,
        maximum_source_sink_distance,
        context,
        source_register_types,
        {}, // TODO: Support via-value-of for crtex frames
        {std::cref(frame)},
        via_type_of_features_added);

    if (propagated.is_bottom()) {
      continue;
    }

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
      auto instantiated_name = canonical_name.instantiate(
          propagated.callee(), via_type_of_features_added);
      if (!instantiated_name) {
        continue;
      }
      instantiated_names.add(*instantiated_name);
    }

    auto canonical_callee_port =
        propagated.callee_port().canonicalize_for_method(propagated.callee());

    // All fields should be propagated like other frames, except the crtex
    // fields. Ideally, origins should contain the canonical names as well,
    // but canonical names are strings and cannot be stored in MethodSet.
    // Frame is not propagated if none of the canonical names instantiated
    // successfully.
    if (instantiated_names.is_value() &&
        !instantiated_names.elements().empty()) {
      result.add(Frame(
          kind_,
          canonical_callee_port,
          propagated.callee(),
          propagated.field_callee(),
          propagated.call_position(),
          /* distance (always leaves for crtex frames) */ 0,
          propagated.origins(),
          propagated.field_origins(),
          propagated.inferred_features(),
          propagated.locally_inferred_features(),
          propagated.user_features(),
          propagated.via_type_of_ports(),
          propagated.via_value_of_ports(),
          propagated.local_positions(),
          /* canonical_names */ instantiated_names));
    }
  }

  return result;
}

} // namespace marianatrench
