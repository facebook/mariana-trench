/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/AbstractDomain.h>
#include <sparta/PatriciaTreeMapAbstractPartition.h>

#include <sparta/FlattenIterator.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/Position.h>
#include <mariana-trench/TaintConfig.h>
#include <mariana-trench/TransformsFactory.h>

/**
 * Includes expected constructors and top/bottom definitions for actual classes
 * deriving from FramesMap.
 */
#define INCLUDE_DERIVED_FRAMES_MAP_CONSTRUCTORS(Derived, Base, MapProperties) \
  explicit Derived() : Base() {}                                              \
                                                                              \
  explicit Derived(MapProperties callee_key, FramesByKey frames)              \
      : Base(callee_key, frames) {}                                           \
                                                                              \
  explicit Derived(std::initializer_list<TaintConfig> configs)                \
      : Base(configs) {}                                                      \
                                                                              \
  static Derived bottom() {                                                   \
    return Derived(MapProperties::make_default(), FramesByKey::bottom());     \
  }                                                                           \
                                                                              \
  static Derived top() {                                                      \
    return Derived(MapProperties::make_default(), FramesByKey::top());        \
  }

namespace marianatrench {

/**
 * Taint's frames are internally stored as a map-of-map-of-map-*. Many
 * operations simply forward the call to the next level until it gets
 * to the leaf Frame. This class implements the forwarding for applicable
 * methods. The following are the expected templates:
 *
 * Derived:
 *   The actual class that represents this map-of-map. This class
 *   should inherit from FramesMap and is an abstract domain.
 * Key:
 *   The type of the key to the map.
 * Value:
 *   The type that a key maps to. Typically another class deriving from
 *   FramesMap.
 * KeyFromTaintConfig:
 *   A function object that returns the key given a `TaintConfig`.
 *   Implements `Key operator()(const TaintConfig&) const`.
 * MapProperties:
 *   A type containing properties common to all frames within this map.
 *   Typically a key to an outer-map containing this object. Not an abstract
 *   domain, but must provide the following, which represents the value this
 *   property takes on when the map is top/bottom.
 *     MapProperties(const TaintConfig&)
 *     static MapProperties make_default()
 *     bool is_default()
 *     void set_to_default()
 */
template <
    typename Derived,
    typename Key,
    typename Value,
    typename KeyFromTaintConfig,
    typename MapProperties>
class FramesMap : public sparta::AbstractDomain<Derived> {
 protected:
  using FramesByKey = sparta::PatriciaTreeMapAbstractPartition<Key, Value>;

 private:
  struct KeyToFramesMapDereference {
    using Reference = typename std::iterator_traits<
        typename std::unordered_map<Key, Value>::const_iterator>::reference;

    static typename Value::iterator begin(Reference iterator) {
      return iterator.second.begin();
    }
    static typename Value::iterator end(Reference iterator) {
      return iterator.second.end();
    }
  };

  using ConstIterator = sparta::FlattenIterator<
      /* OuterIterator */ typename FramesByKey::MapType::iterator,
      /* InnerIterator */ typename Value::iterator,
      KeyToFramesMapDereference>;

 public:
  // C++ container concept member types
  using iterator = ConstIterator;
  using const_iterator = ConstIterator;
  using value_type = Frame;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const Frame&;
  using const_pointer = const Frame*;

 protected:
  explicit FramesMap(MapProperties properties, FramesByKey frames)
      : properties_(std::move(properties)), frames_(std::move(frames)) {}

 public:
  /* Create the bottom (i.e, empty) frame set. */
  FramesMap()
      : properties_(MapProperties::make_default()),
        frames_(FramesByKey::bottom()) {}

  explicit FramesMap(std::initializer_list<TaintConfig> configs) : FramesMap() {
    for (const auto& config : configs) {
      add(config);
    }
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(FramesMap)

  bool is_bottom() const {
    return frames_.is_bottom();
  }

  bool is_top() const {
    return frames_.is_top();
  }

  void set_to_bottom() {
    properties_.set_to_default();
    frames_.set_to_bottom();
  }

  void set_to_top() {
    properties_.set_to_default();
    frames_.set_to_top();
  }

  bool leq(const FramesMap& other) const {
    mt_assert(
        is_bottom() || other.is_bottom() || properties_ == other.properties_);
    return frames_.leq(other.frames_);
  }

  bool equals(const FramesMap& other) const {
    mt_assert(
        is_bottom() || other.is_bottom() || properties_ == other.properties_);
    return frames_.equals(other.frames_);
  }

  void join_with(const FramesMap& other) {
    mt_if_expensive_assert(auto previous = *this);

    if (is_bottom()) {
      mt_assert(properties_.is_default());
      properties_ = other.properties_;
    }
    mt_assert(other.is_bottom() || properties_ == other.properties_);

    frames_.join_with(other.frames_);

    mt_expensive_assert(previous.leq(*this) && other.leq(*this));
  }

  void widen_with(const FramesMap& other) {
    mt_if_expensive_assert(auto previous = *this);

    if (is_bottom()) {
      mt_assert(properties_.is_default());
      properties_ = other.properties_;
    }
    mt_assert(other.is_bottom() || properties_ == other.properties_);

    frames_.widen_with(other.frames_);

    mt_expensive_assert(previous.leq(*this) && other.leq(*this));
  }

  void meet_with(const FramesMap& other) {
    if (is_bottom()) {
      mt_assert(properties_.is_default());
      properties_ = other.properties_;
    }
    mt_assert(other.is_bottom() || properties_ == other.properties_);

    frames_.meet_with(other.frames_);
  }

  void narrow_with(const FramesMap& other) {
    if (is_bottom()) {
      mt_assert(properties_.is_default());
      properties_ = other.properties_;
    }
    mt_assert(other.is_bottom() || properties_ == other.properties_);

    frames_.narrow_with(other.frames_);
  }

  void difference_with(const FramesMap& other) {
    if (is_bottom()) {
      mt_assert(properties_.is_default());
      properties_ = other.properties_;
    }
    mt_assert(other.is_bottom() || properties_ == other.properties_);

    frames_.difference_like_operation(
        other.frames_, [](Value left, const Value& right) {
          left.difference_with(right);
          return left;
        });
  }

  bool empty() const {
    return frames_.is_bottom();
  }

  void add(const TaintConfig& config) {
    if (properties_.is_default()) {
      properties_ = MapProperties(config);
    } else {
      mt_assert(properties_ == MapProperties(config));
    }
    frames_.update(KeyFromTaintConfig()(config), [&config](Value old_frames) {
      old_frames.add(config);
      return old_frames;
    });
  }

  template <typename Operation> // Value(Value)
  void map_frames(Operation&& f) {
    frames_.map(std::forward<Operation>(f));
  }

  template <typename Function> // Frame(Frame)
  void map(Function&& f) {
    static_assert(std::is_same_v<decltype(f(std::declval<Frame&&>())), Frame>);

    map_frames([f = std::forward<Function>(f)](Value frames) {
      frames.map(f);
      return frames;
    });
  }

  template <typename Predicate> // bool(const Frame&)
  void filter(Predicate&& predicate) {
    static_assert(
        std::
            is_same_v<decltype(predicate(std::declval<const Frame&>())), bool>);

    map_frames([predicate = std::forward<Predicate>(predicate)](Value frames) {
      frames.filter(predicate);
      return frames;
    });
  }

  ConstIterator begin() const {
    return ConstIterator(frames_.bindings().begin(), frames_.bindings().end());
  }

  ConstIterator end() const {
    return ConstIterator(frames_.bindings().end(), frames_.bindings().end());
  }

  void set_origins_if_empty(const MethodSet& origins) {
    map_frames([&origins](Value frames) {
      frames.set_origins_if_empty(origins);
      return frames;
    });
  }

  void set_field_origins_if_empty(const Field* field) {
    map_frames([field](Value frames) {
      frames.set_field_origins_if_empty(field);
      return frames;
    });
  }

  void add_locally_inferred_features(const FeatureMayAlwaysSet& features) {
    if (features.empty()) {
      return;
    }

    map_frames([&features](Value frames) {
      frames.add_locally_inferred_features(features);
      return frames;
    });
  }

  LocalPositionSet local_positions() const {
    auto result = LocalPositionSet::bottom();
    for (const auto& [_, frames] : frames_.bindings()) {
      result.join_with(frames.local_positions());
    }
    return result;
  }

  void set_local_positions(const LocalPositionSet& positions) {
    map_frames([&positions](Value frames) {
      frames.set_local_positions(positions);
      return frames;
    });
  }

  template <typename TransformKind, typename AddFeatures>
  void transform_kind_with_features(
      TransformKind&& transform_kind, // std::vector<const Kind*>(const Kind*)
      AddFeatures&& add_features // FeatureMayAlwaysSet(const Kind*)
  ) {
    map_frames(
        [transform_kind = std::forward<TransformKind>(transform_kind),
         add_features = std::forward<AddFeatures>(add_features)](Value frames) {
          frames.transform_kind_with_features(transform_kind, add_features);
          return frames;
        });
  }

  void filter_invalid_frames(
      const std::function<
          bool(const Method* MT_NULLABLE, const AccessPath&, const Kind*)>&
          is_valid) {
    map_frames([&is_valid](Value frames) {
      frames.filter_invalid_frames(is_valid);
      return frames;
    });
  }

  bool contains_kind(const Kind* kind) const {
    auto frames_iterator = frames_.bindings();
    return std::any_of(
        frames_iterator.begin(),
        frames_iterator.end(),
        [kind](const std::pair<Key, Value>& frames_pair) {
          return frames_pair.second.contains_kind(kind);
        });
  }

  template <class T>
  std::unordered_map<T, Derived> partition_by_kind(
      const std::function<T(const Kind*)>& map_kind) const {
    std::unordered_map<T, Derived> result;
    for (const auto& [key, frames] : frames_.bindings()) {
      auto frames_partitioned = frames.partition_by_kind(map_kind);

      for (const auto& [mapped_value, value_frames] : frames_partitioned) {
        result[mapped_value].join_with(
            Derived(properties_, FramesByKey{std::pair(key, value_frames)}));
      }
    }
    return result;
  }

  FeatureMayAlwaysSet features_joined() const {
    auto features = FeatureMayAlwaysSet::bottom();
    for (const auto& [_, frames] : frames_.bindings()) {
      features.join_with(frames.features_joined());
    }
    return features;
  }

 protected:
  MapProperties properties_;
  FramesByKey frames_;
};

} // namespace marianatrench
