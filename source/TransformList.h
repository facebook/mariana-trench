/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/iterator/transform_iterator.hpp>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/SanitizerSetTransform.h>
#include <mariana-trench/Transform.h>
#include <mariana-trench/TransformKind.h>

namespace marianatrench {

/**
 * Represents an ordered list of Transform operations applied to a taint kind.
 * Each application of a Transform operation leads to a creation of a new
 * `TransformKind`. `TransformKind` internally uses `TransformList`s to keep
 * track of the sequence of transformations that have been applied to a base
 * kind.
 */
class TransformList final {
 private:
  using List = std::vector<const Transform*>;

 public:
  using ConstIterator = List::const_iterator;
  using ConstReverseIterator = List::const_reverse_iterator;

  // The reason for not reusing `transforms::TransformDirection` is that it
  // represents the direction of propagation, while we need a enum that encodes
  // the direction of transforms application in the list. For example, even if
  // we are doing Forward propagation (with
  // `transforms::TransformDirection::Forward`), the transform list is still
  // applied in the reverse order on the source. Reusing the enum would cause
  // great ambiguity.
  enum class ApplicationDirection { Forward, Backward };

  /* A non-owning range view of the sanitizers in the list. */
  template <ApplicationDirection Direction>
  class SanitizerRange {
   private:
    using TransformListIterator = std::conditional_t<
        Direction == ApplicationDirection::Forward,
        ConstIterator,
        ConstReverseIterator>;

    /**
     * Functor used by boost::transform_iterator to convert a const Transform*
     * to a const SanitizerSetTransform*.
     */
    struct GetAsSanitizerSetTransform {
      const SanitizerSetTransform* operator()(
          const Transform* transform) const {
        // Only used by SanitizerRange, therefore safe to static_cast.
        return static_cast<const SanitizerSetTransform*>(transform);
      }
    };

   public:
    using Iterator = boost::
        transform_iterator<GetAsSanitizerSetTransform, TransformListIterator>;

    static constexpr ApplicationDirection direction = Direction;

    SanitizerRange(
        const TransformListIterator& begin,
        const TransformListIterator& end)
        : iterator_pair_(
              Iterator(begin, GetAsSanitizerSetTransform()),
              Iterator(end, GetAsSanitizerSetTransform())) {}

    INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(SanitizerRange)

    Iterator begin() const {
      return iterator_pair_.first;
    }

    Iterator end() const {
      return iterator_pair_.second;
    }

    bool empty() const {
      return begin() == end();
    }

    std::size_t size() const {
      return std::distance(begin(), end());
    }

   private:
    std::pair<Iterator, Iterator> iterator_pair_;
  };

 public:
  TransformList() = default;

  explicit TransformList(List transforms)
      : transforms_(std::move(transforms)) {}

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(TransformList)

 private:
  explicit TransformList(ConstIterator begin, ConstIterator end)
      : transforms_(List{begin, end}) {}

  explicit TransformList(
      const std::vector<std::string>& transforms,
      Context& context);

  static TransformList reverse_of(const TransformList* transforms);

  static TransformList discard_sanitizers(const TransformList* transforms);

  /**
   * Helper for locating the leading sanitizers (i.e. the adjacent sanitizers
   * at the start/end of the list, given the direction).
   * The result is in the form of a non-owning range implemented by an iterator
   * pair, denoting the begin and end.
   */
  template <ApplicationDirection Direction>
  SanitizerRange<Direction> find_consecutive_sanitizers() const {
    if constexpr (Direction == ApplicationDirection::Forward) {
      // Use cbegin and cend
      auto sanitizer_begin = transforms_.cbegin();
      auto sanitizer_end = std::find_if(
          sanitizer_begin, transforms_.cend(), [](const Transform* transform) {
            return !transform->is<SanitizerSetTransform>();
          });
      return SanitizerRange<Direction>(sanitizer_begin, sanitizer_end);
    } else {
      // Use crbegin and crend
      auto sanitizer_rbegin = transforms_.crbegin();
      auto sanitizer_rend = std::find_if(
          sanitizer_rbegin,
          transforms_.crend(),
          [](const Transform* transform) {
            return !transform->is<SanitizerSetTransform>();
          });
      return SanitizerRange<Direction>(sanitizer_rbegin, sanitizer_rend);
    }
  }

 public:
  bool operator==(const TransformList& other) const {
    return transforms_ == other.transforms_;
  }

  ConstIterator begin() const {
    return transforms_.cbegin();
  }

  ConstIterator end() const {
    return transforms_.cend();
  }

  std::size_t size() const {
    return transforms_.size();
  }

  /**
   * Checks whether the consecutive sanitizers at begining or end of the list
   * would sanitize the given kind
   */
  template <ApplicationDirection Direction>
  bool sanitizes(const Kind* kind, transforms::TransformDirection direction)
      const {
    if (const auto* transform_kind = kind->as<TransformKind>()) {
      // The only place where this function can be called with
      // ApplicationDirection::Forward is during rule matching, in which case
      // the kind argument is never a TransformKind.
      if constexpr (Direction == ApplicationDirection::Forward) {
        mt_unreachable();
      }

      if (transform_kind->has_non_sanitize_transform()) {
        return false;
      }

      // Otherwise, check whether `this` can sanitize the base_kind (since the
      // existing sanitizers in transform_kind definitely cannot sanitize
      // base_kind)
      kind = transform_kind->base_kind();
    }

    const auto sanitizer_range = find_consecutive_sanitizers<Direction>();
    return std::any_of(
        sanitizer_range.begin(),
        sanitizer_range.end(),
        [kind, direction](const SanitizerSetTransform* sanitizer) {
          return sanitizer->kinds().contains(
              SourceSinkKind::from_transform_direction(kind, direction));
        });
  }

  bool has_source_as_transform() const;

  bool has_non_sanitize_transform() const;

  std::string to_trace_string() const;
  static TransformList from_trace_string(
      const std::string& trace_string,
      Context& context);

  static TransformList from_json(
      const Json::Value& transforms,
      Context& context);

  static TransformList from_kind(const Kind* kind, Context& context);

  static TransformList concat(
      const TransformList* left,
      const TransformList* right);

  static TransformList canonicalize(
      const TransformList* transforms,
      const TransformsFactory& transforms_factory);

  static TransformList filter_global_sanitizers(
      const TransformList* incoming,
      const TransformList* existing_global,
      const TransformsFactory& transforms_factory);

 private:
  friend class TransformsFactory;

 private:
  List transforms_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::TransformList> {
  std::size_t operator()(const marianatrench::TransformList& transforms) const {
    return boost::hash_range(transforms.begin(), transforms.end());
  }
};
