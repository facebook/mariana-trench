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

#include <AbstractDomain.h>
#include <HashedAbstractPartition.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/FramesMap.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/TaintConfig.h>

namespace marianatrench {

class CalleeInterval {
 public:
  explicit CalleeInterval(
      ClassIntervals::Interval interval,
      bool preserves_type_context);

  explicit CalleeInterval(const TaintConfig& config);

  explicit CalleeInterval(const Frame& frame);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CalleeInterval)

  bool operator==(const CalleeInterval& other) const {
    return interval_ == other.interval_ &&
        preserves_type_context_ == other.preserves_type_context_;
  }

  const ClassIntervals::Interval& interval() const {
    return interval_;
  }

  bool preserves_type_context() const {
    return preserves_type_context_;
  }

 private:
  ClassIntervals::Interval interval_;
  bool preserves_type_context_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::CalleeInterval> {
  std::size_t operator()(
      const marianatrench::CalleeInterval& callee_interval) const {
    std::size_t seed = 0;
    boost::hash_combine(
        seed,
        std::hash<marianatrench::ClassIntervals::Interval>()(
            callee_interval.interval()));
    boost::hash_combine(
        seed, std::hash<bool>()(callee_interval.preserves_type_context()));
    return seed;
  }
};

namespace marianatrench {

/**
 * Represents a set of frames with the same kind.
 */
class KindFrames final : public sparta::AbstractDomain<KindFrames> {
 private:
  using FramesByInterval =
      sparta::HashedAbstractPartition<CalleeInterval, Frame>;

 private:
  struct GetFrameReference {
    using Reference = typename std::iterator_traits<
        typename std::unordered_map<CalleeInterval, Frame>::const_iterator>::
        reference;

    const Frame& operator()(Reference element) const {
      return element.second;
    }
  };

  using ConstIterator = boost::transform_iterator<
      GetFrameReference,
      typename std::unordered_map<CalleeInterval, Frame>::const_iterator>;

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
    // TODO(T158171922): Implement map in HashedAbstractPartition to avoid copy.
    if (frames_.is_top()) {
      return;
    }

    FramesByInterval new_frames;
    for (const auto& [key, value] : frames_.bindings()) {
      auto new_value = f(value);
      // The map operation must not change the kind, unless it is
      // Frame::bottom(), in which case, the entry will be dropped from the map
      mt_assert(new_value.is_bottom() || new_value.kind() == kind_);
      new_frames.set(key, new_value);
    }

    if (new_frames.is_bottom()) {
      set_to_bottom();
    } else {
      frames_ = std::move(new_frames);
    }
  }

  template <typename Predicate> // bool(const Frame&)
  void filter(Predicate&& predicate) {
    static_assert(
        std::is_same_v<decltype(predicate(std::declval<const Frame>())), bool>);
    map([predicate = std::forward<Predicate>(predicate)](Frame frame) {
      if (!predicate(frame)) {
        return Frame::bottom();
      }
      return frame;
    });
  }

  ConstIterator begin() const {
    return boost::make_transform_iterator(
        frames_.bindings().cbegin(), GetFrameReference());
  }

  ConstIterator end() const {
    return boost::make_transform_iterator(
        frames_.bindings().cend(), GetFrameReference());
  }

  void set_origins_if_empty(const MethodSet& origins);

  void set_field_origins_if_empty_with_field_callee(const Field* field);

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
      std::vector<const Feature*>& via_type_of_features_added) const;

  /**
   * Similar to propagate but intended to be used for CRTEX frames.
   * Caller is responsible for determining whether the frame a CRTEX
   * leaf (using the callee port).
   */
  KindFrames propagate_crtex_leaf_frames(
      const Method* callee,
      const AccessPath& canonical_callee_port,
      const Position* call_position,
      const FeatureMayAlwaysSet& locally_inferred_features,
      int maximum_source_sink_distance,
      Context& context,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types)
      const;

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
