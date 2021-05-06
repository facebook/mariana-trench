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
    const std::vector<const DexType * MT_NULLABLE>& source_register_types)
    const {
  if (is_bottom()) {
    return FrameSet::bottom();
  }

  auto partitioned =
      partition_map<bool>([](const Frame& frame) { return frame.is_crtex(); });

  auto frames = propagate_crtex_frames(
      callee,
      callee_port,
      call_position,
      maximum_source_sink_distance,
      context,
      source_register_types,
      partitioned[true]);

  // Non-CRTEX frames can be joined into the same callee
  auto non_crtex_frame = propagate_frames(
      callee,
      callee_port,
      call_position,
      maximum_source_sink_distance,
      context,
      source_register_types,
      partitioned[false]);
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

        leaves.add(Frame(
            frame.kind(),
            frame.callee_port(),
            /* callee */ nullptr,
            /* call_position */ position,
            /* distance */ 0,
            frame.origins(),
            frame.features(),
            /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
            /* user_features */ FeatureSet::bottom(),
            /* via_type_of_ports */ {},
            frame.local_positions(),
            /* canonical_names */ {}));
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

Frame FrameSet::propagate_frames(
    const Method* callee,
    const AccessPath& callee_port,
    const Position* call_position,
    int maximum_source_sink_distance,
    Context& context,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    std::vector<std::reference_wrapper<const Frame>> frames) const {
  int distance = std::numeric_limits<int>::max();
  auto origins = MethodSet::bottom();
  auto inferred_features = FeatureMayAlwaysSet::bottom();

  for (const Frame& frame : frames) {
    if (frame.distance() >= maximum_source_sink_distance) {
      continue;
    }

    distance = std::min(distance, frame.distance() + 1);
    origins.join_with(frame.origins());

    // Note: This merges user features with existing inferred features.
    inferred_features.join_with(frame.features());
    // Materialize via_type_of_ports into features and add them to the
    // inferred features
    if (!frame.via_type_of_ports().is_value() ||
        frame.via_type_of_ports().elements().empty()) {
      continue;
    }
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
      inferred_features.add_always(context.features->get_via_type_of_feature(
          source_register_types[port.parameter_position()]));
    }
  }

  if (distance == std::numeric_limits<int>::max()) {
    return Frame::bottom();
  }

  return Frame(
      kind_,
      callee_port,
      callee,
      call_position,
      distance,
      std::move(origins),
      std::move(inferred_features),
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user_features */ FeatureSet::bottom(),
      /* via_type_of_ports */ {},
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
    auto propagated = propagate_frames(
        callee,
        callee_port,
        call_position,
        maximum_source_sink_distance,
        context,
        source_register_types,
        {std::cref(frame)});

    if (!propagated.is_bottom()) {
      // TODO: Apply the canonical names (which will update the callee)
      result.add(propagated);
    }
  }

  return result;
}

} // namespace marianatrench
