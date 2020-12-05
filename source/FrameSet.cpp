/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/FrameSet.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>

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

void FrameSet::add_features(const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map([&features](Frame& frame) { frame.add_features(features); });
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

void FrameSet::add_features_and_local_position(
    const FeatureMayAlwaysSet& features,
    const Position* MT_NULLABLE position) {
  if (features.empty() && position == nullptr) {
    return;
  }

  map([&features, position](Frame& frame) {
    if (!features.empty()) {
      frame.add_features(features);
    }
    if (position != nullptr) {
      frame.add_local_position(position);
    }
  });
}

Frame FrameSet::propagate(
    const Method* /*caller*/,
    const Method* callee,
    const AccessPath& callee_port,
    const Position* call_position,
    int maximum_source_sink_distance) const {
  if (is_bottom()) {
    return Frame::bottom();
  }

  int distance = std::numeric_limits<int>::max();
  auto origins = MethodSet::bottom();
  auto features = FeatureMayAlwaysSet::bottom();

  for (const auto& [_, position_map] : map_.bindings()) {
    for (const auto& [_, set] : position_map.bindings()) {
      for (const auto& frame : set) {
        if (frame.distance() >= maximum_source_sink_distance) {
          continue;
        }

        distance = std::min(distance, frame.distance() + 1);
        origins.join_with(frame.origins());
        features.join_with(frame.features());
      }
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
      std::move(features),
      /* local_positions */ {});
}

FrameSet FrameSet::attach_position(
    const Position* position,
    const FeatureMayAlwaysSet& extra_features) const {
  FrameSet leaves;

  for (const auto& [_, position_map] : map_.bindings()) {
    for (const auto& [_, set] : position_map.bindings()) {
      for (const auto& frame : set) {
        if (!frame.is_leaf()) {
          continue;
        }

        auto features = frame.features();
        features.add(extra_features);

        leaves.add(Frame(
            frame.kind(),
            frame.callee_port(),
            /* callee */ nullptr,
            /* call_position */ position,
            /* distance */ 0,
            frame.origins(),
            std::move(features),
            frame.local_positions()));
      }
    }
  }

  return leaves;
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

} // namespace marianatrench
