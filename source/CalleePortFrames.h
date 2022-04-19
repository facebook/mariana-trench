/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/iterator/transform_iterator.hpp>
#include <initializer_list>
#include <ostream>

#include <json/json.h>

#include <AbstractDomain.h>
#include <PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/FlattenIterator.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/GroupHashedSetAbstractDomain.h>

namespace marianatrench {

/**
 * Represents a set of frames with the same call position.
 * Based on its position in `Taint`, it is expected that all frames within
 * this class have the same callee and call position.
 */
class CalleePortFrames final : public sparta::AbstractDomain<CalleePortFrames> {
 private:
  using Frames =
      GroupHashedSetAbstractDomain<Frame, Frame::GroupHash, Frame::GroupEqual>;
  using FramesByKind =
      sparta::PatriciaTreeMapAbstractPartition<const Kind*, Frames>;

 private:
  // Iterator based on `FlattenIterator`.

  struct KindToFramesMapDereference {
    static Frames::iterator begin(const std::pair<const Kind*, Frames>& pair) {
      return pair.second.begin();
    }
    static Frames::iterator end(const std::pair<const Kind*, Frames>& pair) {
      return pair.second.end();
    }
  };

  using ConstIterator = FlattenIterator<
      /* OuterIterator */ FramesByKind::MapType::iterator,
      /* InnerIterator */ Frames::iterator,
      KindToFramesMapDereference>;

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
  explicit CalleePortFrames(AccessPath callee_port, FramesByKind frames)
      : callee_port_(std::move(callee_port)), frames_(std::move(frames)) {}

 public:
  /**
   * Create the bottom (i.e, empty) frame set. Value of callee_port_ doesn't
   * matter, so we pick some default and Leaf seems like a decent choice.
   */
  CalleePortFrames()
      : callee_port_(Root(Root::Kind::Leaf)), frames_(FramesByKind::bottom()) {}

  explicit CalleePortFrames(std::initializer_list<Frame> frames);

  CalleePortFrames(const CalleePortFrames&) = default;
  CalleePortFrames(CalleePortFrames&&) = default;
  CalleePortFrames& operator=(const CalleePortFrames&) = default;
  CalleePortFrames& operator=(CalleePortFrames&&) = default;

  static CalleePortFrames bottom() {
    return CalleePortFrames(
        /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
        FramesByKind::bottom());
  }

  static CalleePortFrames top() {
    return CalleePortFrames(
        /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
        FramesByKind::top());
  }

  bool is_bottom() const override {
    return frames_.is_bottom();
  }

  bool is_top() const override {
    return frames_.is_top();
  }

  void set_to_bottom() override {
    callee_port_ = AccessPath(Root(Root::Kind::Leaf));
    frames_.set_to_bottom();
  }

  void set_to_top() override {
    callee_port_ = AccessPath(Root(Root::Kind::Leaf));
    frames_.set_to_top();
  }

  bool empty() const {
    return frames_.is_bottom();
  }

  const AccessPath& callee_port() const {
    return callee_port_;
  }

  void add(const Frame& frame);

  bool leq(const CalleePortFrames& other) const override;

  bool equals(const CalleePortFrames& other) const override;

  void join_with(const CalleePortFrames& other) override;

  void widen_with(const CalleePortFrames& other) override;

  void meet_with(const CalleePortFrames& other) override;

  void narrow_with(const CalleePortFrames& other) override;

  void difference_with(const CalleePortFrames& other);

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
  CalleePortFrames propagate(
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments)
      const;

  /* Return the set of leaf frames with the given position. */
  // Not applicable to callee frames. This operates on position, which should
  // be done in CallPositionFrames.
  // CalleePortFrames attach_position(const Position* position) const;

  CalleePortFrames transform_kind_with_features(
      const std::function<std::vector<const Kind*>(const Kind*)>&,
      const std::function<FeatureMayAlwaysSet(const Kind*)>&) const;

  /**
   * Returns a new instance of `CalleePortFrames` in which `path_element` is
   * appended to the port.
   *
   * CAVEAT: When calling this, all underlying frames are expected to have
   * kind == artificial_source(). Use `partition_by_kind` to achieve this.
   *
   * TODO(T91357916): `CallPositionFrames` has an `append_callee_port` which
   * only appends the port if the Frame's kind passes a certain filter. This
   * behavior is not supported here. `CallPositionFrames` should call
   * `partition_by_kind` followed by `append_callee_port` to achieve the
   * desired behavior when it switches over to using `CalleePortFrames`.
   */
  CalleePortFrames append_callee_port(Path::Element path_element) const;

  // TODO(T91357916): To be implemented.
  // void filter_invalid_frames(
  //     const std::function<
  //         bool(const Method* MT_NULLABLE, const AccessPath&, const Kind*)>&
  //         is_valid);

  bool contains_kind(const Kind*) const;

  template <class T>
  std::unordered_map<T, CalleePortFrames> partition_by_kind(
      const std::function<T(const Kind*)>& map_kind) const {
    std::unordered_map<T, CalleePortFrames> result;

    for (const auto& [kind, kind_frames] : frames_.bindings()) {
      T mapped_value = map_kind(kind);
      auto new_frames = CalleePortFrames(
          callee_port_, FramesByKind{std::pair(kind, kind_frames)});

      auto existing = result.find(mapped_value);
      auto existing_or_bottom = existing == result.end()
          ? CalleePortFrames::bottom()
          : existing->second;
      existing_or_bottom.join_with(new_frames);

      result[mapped_value] = existing_or_bottom;
    }
    return result;
  }

  template <class T>
  std::unordered_map<T, std::vector<std::reference_wrapper<const Frame>>>
  partition_map(const std::function<T(const Frame&)>& map) const {
    std::unordered_map<T, std::vector<std::reference_wrapper<const Frame>>>
        result;
    for (const auto& [_, frames] : frames_.bindings()) {
      for (const auto& frame : frames) {
        auto value = map(frame);
        result[value].push_back(std::cref(frame));
      }
    }

    return result;
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const CalleePortFrames& frames);

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

  CalleePortFrames propagate_crtex_frames(
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      std::vector<std::reference_wrapper<const Frame>> frames) const;

 private:
  AccessPath callee_port_;
  FramesByKind frames_;

  // TODO(T91357916): Move local_positions and local_features here from `Frame`.
};

} // namespace marianatrench
