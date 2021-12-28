/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <initializer_list>
#include <ostream>

#include <json/json.h>

#include <AbstractDomain.h>
#include <PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/FlattenIterator.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/GroupHashedSetAbstractDomain.h>

namespace marianatrench {

/* Represents a set of frames with the same kind. */
class FrameSet final : public sparta::AbstractDomain<FrameSet> {
 private:
  using Set =
      GroupHashedSetAbstractDomain<Frame, Frame::GroupHash, Frame::GroupEqual>;
  using CallPositionToSetMap =
      sparta::PatriciaTreeMapAbstractPartition<const Position*, Set>;
  using CalleeToCallPositionToSetMap = sparta::
      PatriciaTreeMapAbstractPartition<const Method*, CallPositionToSetMap>;

 private:
  // Iterator based on `FlattenIterator`.

  struct CallPositionToSetMapDereference {
    static Set::iterator begin(const std::pair<const Position*, Set>& pair) {
      return pair.second.begin();
    }
    static Set::iterator end(const std::pair<const Position*, Set>& pair) {
      return pair.second.end();
    }
  };

  using CallPositionToSetMapIterator = FlattenIterator<
      /* OuterIterator */ CallPositionToSetMap::MapType::iterator,
      /* InnerIterator */ Set::iterator,
      CallPositionToSetMapDereference>;

  struct CalleeToCallPositionToSetMapDereference {
    static CallPositionToSetMapIterator begin(
        const std::pair<const Method*, CallPositionToSetMap>& pair) {
      return CallPositionToSetMapIterator(
          pair.second.bindings().begin(), pair.second.bindings().end());
    }
    static CallPositionToSetMapIterator end(
        const std::pair<const Method*, CallPositionToSetMap>& pair) {
      return CallPositionToSetMapIterator(
          pair.second.bindings().end(), pair.second.bindings().end());
    }
  };

  using ConstIterator = FlattenIterator<
      /* OuterReference */ CalleeToCallPositionToSetMap::MapType::iterator,
      /* InnerIterator */ CallPositionToSetMapIterator,
      CalleeToCallPositionToSetMapDereference>;

 public:
  // C++ container concept member types
  using iterator = ConstIterator;
  using const_iterator = ConstIterator;
  using value_type = Frame;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const Frame&;
  using const_pointer = const Frame*;

 private:
  explicit FrameSet(
      const Kind* MT_NULLABLE kind,
      CalleeToCallPositionToSetMap map)
      : kind_(kind), map_(std::move(map)) {}

 public:
  /* Create the bottom (i.e, empty) frame set. */
  FrameSet() : kind_(nullptr), map_(CalleeToCallPositionToSetMap::bottom()) {}

  explicit FrameSet(std::initializer_list<Frame> frames);

  FrameSet(const FrameSet&) = default;
  FrameSet(FrameSet&&) = default;
  FrameSet& operator=(const FrameSet&) = default;
  FrameSet& operator=(FrameSet&&) = default;

  static FrameSet bottom() {
    return FrameSet(/* kind */ nullptr, CalleeToCallPositionToSetMap::bottom());
  }

  static FrameSet top() {
    return FrameSet(/* kind */ nullptr, CalleeToCallPositionToSetMap::top());
  }

  bool is_bottom() const override {
    return map_.is_bottom();
  }

  bool is_top() const override {
    return map_.is_top();
  }

  void set_to_bottom() override {
    kind_ = nullptr;
    map_.set_to_bottom();
  }

  void set_to_top() override {
    kind_ = nullptr;
    map_.set_to_top();
  }

  bool empty() const {
    return map_.is_bottom();
  }

  const Kind* MT_NULLABLE kind() const {
    return kind_;
  }

  bool is_artificial_sources() const {
    return kind_ == Kinds::artificial_source();
  }

  void add(const Frame& frame);

  bool leq(const FrameSet& other) const override;

  bool equals(const FrameSet& other) const override;

  void join_with(const FrameSet& other) override;

  void widen_with(const FrameSet& other) override;

  void meet_with(const FrameSet& other) override;

  void narrow_with(const FrameSet& other) override;

  void difference_with(const FrameSet& other);

  void map(const std::function<void(Frame&)>& f);

  void filter(const std::function<bool(const Frame&)>& predicate);

  ConstIterator begin() const {
    return ConstIterator(map_.bindings().begin(), map_.bindings().end());
  }

  ConstIterator end() const {
    return ConstIterator(map_.bindings().end(), map_.bindings().end());
  }

  void add_inferred_features(const FeatureMayAlwaysSet& features);

  LocalPositionSet local_positions() const;

  void add_local_position(const Position* position);

  void set_local_positions(const LocalPositionSet& positions);

  void add_inferred_features_and_local_position(
      const FeatureMayAlwaysSet& features,
      const Position* MT_NULLABLE position);

  FeatureMayAlwaysSet features_joined() const;

  /**
   * Propagate the taint from the callee to the caller.
   *
   * Return bottom if the taint should not be propagated.
   */
  FrameSet propagate(
      const Method* caller,
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments)
      const;

  /* Return the set of leaf frames with the given position. */
  FrameSet attach_position(const Position* position) const;

  /**
   * Convert the kind of these frames into the given kind.
   *
   * Return A new FrameSet, identical in everyway except in "kind".
   */
  FrameSet with_kind(const Kind* kind) const;

  template <class T>
  std::unordered_map<T, std::vector<std::reference_wrapper<const Frame>>>
  partition_map(const std::function<T(const Frame&)>& map) const;

  static FrameSet from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  friend std::ostream& operator<<(std::ostream& out, const FrameSet& frames);

 private:
  Frame propagate_frames(
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments,
      std::vector<std::reference_wrapper<const Frame>> frames,
      std::vector<const Feature*>& via_type_of_features_added) const;

  FrameSet propagate_crtex_frames(
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      std::vector<std::reference_wrapper<const Frame>> frames) const;

 private:
  const Kind* MT_NULLABLE kind_;
  CalleeToCallPositionToSetMap map_;
};

} // namespace marianatrench
