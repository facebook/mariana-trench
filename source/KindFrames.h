/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <initializer_list>
#include <ostream>

#include <boost/iterator/transform_iterator.hpp>
#include <json/json.h>

#include <sparta/AbstractDomain.h>
#include <sparta/HashedAbstractPartition.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/CallClassIntervalContext.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/FramesMap.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/TaintConfig.h>

namespace marianatrench {

/**
 * Represents a set of frames with the same kind.
 */
class KindFrames final : public sparta::AbstractDomain<KindFrames> {
 private:
  using FramesByInterval =
      sparta::HashedAbstractPartition<CallClassIntervalContext, Frame>;

 private:
  struct GetFrameReference {
    using Reference = typename std::iterator_traits<typename std::unordered_map<
        CallClassIntervalContext,
        Frame>::const_iterator>::reference;

    const Frame& operator()(Reference element) const {
      return element.second;
    }
  };

  using ConstIterator = boost::transform_iterator<
      GetFrameReference,
      typename std::unordered_map<CallClassIntervalContext, Frame>::
          const_iterator>;

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
  explicit KindFrames(const Kind* MT_NULLABLE kind, FramesByInterval frames)
      : kind_(kind), frames_(std::move(frames)) {
    // kind == nullptr iff is_bottom
    mt_assert(frames_.is_bottom() || kind_ != nullptr);
    mt_assert(!frames_.is_bottom() || kind_ == nullptr);
  }

 public:
  /**
   * Create the bottom (i.e, empty) frame set.
   */
  KindFrames() : kind_(nullptr), frames_(FramesByInterval::bottom()) {}

  explicit KindFrames(std::initializer_list<TaintConfig> configs);

  explicit KindFrames(const Frame& frame);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(KindFrames)

  bool is_bottom() const;
  bool is_top() const;
  void set_to_bottom();
  void set_to_top();
  bool leq(const KindFrames& other) const;
  bool equals(const KindFrames& other) const;
  void join_with(const KindFrames& other);
  void widen_with(const KindFrames& other);
  void meet_with(const KindFrames& other);
  void narrow_with(const KindFrames& other);

  const Kind* MT_NULLABLE kind() const {
    return kind_;
  }

  static KindFrames bottom() {
    return KindFrames();
  }

  static KindFrames top() {
    // Top is not used for this domain.
    mt_unreachable();
  }

  void add(const TaintConfig& config);

  void add(const Frame& frame);

  void difference_with(const KindFrames& other);

  template <typename Function> // Frame(Frame)
  void map(Function&& f) {
    static_assert(std::is_same_v<decltype(f(std::declval<Frame&&>())), Frame>);

    frames_.map([f = std::forward<Function>(f)](Frame* frame) -> void {
      // The map operation must not change the kind, unless it is
      // Frame::bottom(), in which case, the entry will be dropped from the map
      *frame = f(std::move(*frame));
    });

    if (frames_.is_bottom()) {
      set_to_bottom();
    }
  }

  template <typename Predicate> // bool(const Frame&)
  void filter(Predicate&& predicate) {
    static_assert(
        std::is_same_v<decltype(predicate(std::declval<const Frame>())), bool>);

    frames_.map(
        [predicate = std::forward<Predicate>(predicate)](Frame* frame) -> void {
          if (!predicate(*frame)) {
            frame->set_to_bottom();
          }
        });

    if (frames_.is_bottom()) {
      set_to_bottom();
    }
  }

  ConstIterator begin() const {
    return boost::make_transform_iterator(
        frames_.bindings().cbegin(), GetFrameReference());
  }

  ConstIterator end() const {
    return boost::make_transform_iterator(
        frames_.bindings().cend(), GetFrameReference());
  }

  /**
   * Appends `path_element` to the output paths of all propagation frames.
   */
  void append_to_propagation_output_paths(Path::Element path_element);

  /**
   * Propagate the taint from the callee to the caller.
   *
   * Return bottom if the taint should not be propagated.
   */
  KindFrames propagate(
      const Method* callee,
      const AccessPath& callee_port,
      const Position* call_position,
      const FeatureMayAlwaysSet& locally_inferred_features,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types,
      const std::vector<std::optional<std::string>>& source_constant_arguments,
      const CallClassIntervalContext& class_interval_context,
      const ClassIntervals::Interval& caller_class_interval) const;

  void filter_invalid_frames(
      const std::function<
          bool(const Method* MT_NULLABLE, const AccessPath&, const Kind*)>&
          is_valid);

  bool contains_kind(const Kind*) const;

  KindFrames with_kind(const Kind* kind) const;

  void add_inferred_features(const FeatureMayAlwaysSet& features);

  friend std::ostream& operator<<(std::ostream& out, const KindFrames& frames);

 private:
  const Kind* MT_NULLABLE kind_;
  FramesByInterval frames_;
};

} // namespace marianatrench
