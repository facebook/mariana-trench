/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <AbstractDomain.h>
#include <PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/CallPositionFrames.h>
#include <mariana-trench/Position.h>

namespace marianatrench {

/**
 * Represents a set of frames with the same call position.
 * Based on its position in `Taint`, it is expected that all frames within
 * this class have the same callee and call position.
 */
class CalleeFrames final : public sparta::AbstractDomain<CalleeFrames> {
 private:
  using FramesByCallPosition = sparta::PatriciaTreeMapAbstractPartition<
      const Position * MT_NULLABLE,
      CallPositionFrames>;

 private:
  // Iterator based on `FlattenIterator`.

  struct CallPositionToFramesMapDereference {
    static CallPositionFrames::iterator begin(
        const std::pair<const Position*, CallPositionFrames>& pair) {
      return pair.second.begin();
    }
    static CallPositionFrames::iterator end(
        const std::pair<const Position*, CallPositionFrames>& pair) {
      return pair.second.end();
    }
  };

  using ConstIterator = FlattenIterator<
      /* OuterIterator */ FramesByCallPosition::MapType::iterator,
      /* InnerIterator */ CallPositionFrames::iterator,
      CallPositionToFramesMapDereference>;

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
  explicit CalleeFrames(
      const Method* MT_NULLABLE callee,
      FramesByCallPosition frames)
      : callee_(callee), frames_(std::move(frames)) {}

 public:
  /* Create the bottom (i.e, empty) frame set. */
  CalleeFrames() : callee_(nullptr), frames_(FramesByCallPosition::bottom()) {}

  explicit CalleeFrames(std::initializer_list<Frame> frames);

  CalleeFrames(const CalleeFrames&) = default;
  CalleeFrames(CalleeFrames&&) = default;
  CalleeFrames& operator=(const CalleeFrames&) = default;
  CalleeFrames& operator=(CalleeFrames&&) = default;

  static CalleeFrames bottom() {
    return CalleeFrames(
        /* callee */ nullptr, FramesByCallPosition::bottom());
  }

  static CalleeFrames top() {
    return CalleeFrames(
        /* callee */ nullptr, FramesByCallPosition::top());
  }

  bool is_bottom() const override {
    return frames_.is_bottom();
  }

  bool is_top() const override {
    return frames_.is_top();
  }

  void set_to_bottom() override {
    callee_ = nullptr;
    frames_.set_to_bottom();
  }

  void set_to_top() override {
    callee_ = nullptr;
    frames_.set_to_top();
  }

  bool empty() const {
    return frames_.is_bottom();
  }

  const Method* MT_NULLABLE callee() const {
    return callee_;
  }

  void add(const Frame& frame);

  bool leq(const CalleeFrames& other) const override;

  bool equals(const CalleeFrames& other) const override;

  void join_with(const CalleeFrames& other) override;

  void widen_with(const CalleeFrames& other) override;

  void meet_with(const CalleeFrames& other) override;

  void narrow_with(const CalleeFrames& other) override;

  void difference_with(const CalleeFrames& other);

  void map(const std::function<void(Frame&)>& f);

  ConstIterator begin() const {
    return ConstIterator(frames_.bindings().begin(), frames_.bindings().end());
  }

  ConstIterator end() const {
    return ConstIterator(frames_.bindings().end(), frames_.bindings().end());
  }

  void add_inferred_features(const FeatureMayAlwaysSet& features);

  LocalPositionSet local_positions() const;

  void add_local_position(const Position* position);

  void set_local_positions(const LocalPositionSet& positions);

  void add_inferred_features_and_local_position(
      const FeatureMayAlwaysSet& features,
      const Position* MT_NULLABLE position);

  /**
   * Propagate the taint from the callee to the caller.
   *
   * Return bottom if the taint should not be propagated.
   */
  CalleeFrames propagate(
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments)
      const;

  /* Return the set of leaf frames with the given position. */
  CalleeFrames attach_position(const Position* position) const;

  void transform_kind_with_features(
      const std::function<std::vector<const Kind*>(const Kind*)>&,
      const std::function<FeatureMayAlwaysSet(const Kind*)>&);

  void append_callee_port_to_artificial_sources(Path::Element path_element);

  void filter_invalid_frames(
      const std::function<
          bool(const Method* MT_NULLABLE, const AccessPath&, const Kind*)>&
          is_valid);

  bool contains_kind(const Kind*) const;

  template <class T>
  std::unordered_map<T, CalleeFrames> partition_by_kind(
      const std::function<T(const Kind*)>& map_kind) const {
    std::unordered_map<T, CalleeFrames> result;
    for (const auto& [position, position_frames] : frames_.bindings()) {
      auto callee_frames_partitioned =
          position_frames.partition_by_kind(map_kind);

      for (const auto& [mapped_value, call_position_frames] :
           callee_frames_partitioned) {
        auto new_frames = CalleeFrames(
            callee_,
            FramesByCallPosition{std::pair(position, call_position_frames)});

        auto existing = result.find(mapped_value);
        auto existing_or_bottom = existing == result.end()
            ? CalleeFrames::bottom()
            : existing->second;
        existing_or_bottom.join_with(new_frames);

        result[mapped_value] = existing_or_bottom;
      }
    }
    return result;
  }

  Json::Value to_json() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const CalleeFrames& frames);

 private:
  const Method* MT_NULLABLE callee_;
  FramesByCallPosition frames_;
};

} // namespace marianatrench
