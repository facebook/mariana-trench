/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

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

  // The reason for not reusing `transforms::TransformDirection` is that it
  // represents the direction of propagation, while we need a enum that encodes
  // the direction of transforms application in the list. For example, even if
  // we are doing Forward propagation (with
  // `transforms::TransformDirection::Forward`), the transform list is still
  // applied in the reverse order on the source. Reusing the enum would cause
  // great ambiguity.
  enum class ApplicationDirection { Forward, Backward };

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
   * The result is in the form of an iterator pair, denoting the begin and end
   * If `Direction == ApplicationDirection::Forward`, this would be a pair of
   * `std::vector<const Transform*>::const_iterator`,
   * If `Direction == ApplicationDirection::Backward`, this would be a pair of
   * `std::vector<const Transform*>::const_reverse_iterator`
   */
  template <ApplicationDirection Direction>
  auto find_consecutive_sanitizers() const {
    if constexpr (Direction == ApplicationDirection::Forward) {
      // Use cbegin and cend
      auto sanitizer_begin = transforms_.cbegin();
      auto sanitizer_end = std::find_if(
          sanitizer_begin, transforms_.cend(), [](const Transform* transform) {
            return !transform->is<SanitizerSetTransform>();
          });
      return std::make_pair(sanitizer_begin, sanitizer_end);
    } else {
      // Use crbegin and crend
      auto sanitizer_rbegin = transforms_.crbegin();
      auto sanitizer_rend = std::find_if(
          sanitizer_rbegin,
          transforms_.crend(),
          [](const Transform* transform) {
            return !transform->is<SanitizerSetTransform>();
          });
      return std::make_pair(sanitizer_rbegin, sanitizer_rend);
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
  bool sanitizes(const Kind* kind) const {
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

    auto [sanitizer_begin, sanitizer_end] =
        find_consecutive_sanitizers<Direction>();
    return std::any_of(
        std::move(sanitizer_begin),
        std::move(sanitizer_end),
        [kind](const Transform* transform) {
          return transform->as<SanitizerSetTransform>()->kinds().contains(kind);
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
