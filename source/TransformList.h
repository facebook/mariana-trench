/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/SanitizeTransform.h>
#include <mariana-trench/Transform.h>

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

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(TransformList)

 private:
  explicit TransformList(List transforms)
      : transforms_(std::move(transforms)) {}

  explicit TransformList(ConstIterator begin, ConstIterator end)
      : transforms_(List{begin, end}) {}

  explicit TransformList(
      const std::vector<std::string>& transforms,
      Context& context);

  static TransformList reverse_of(const TransformList* transforms);

  static TransformList discard_sanitizers(const TransformList* transforms);

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

  bool has_sanitizers() const {
    return std::any_of(begin(), end(), [](const Transform* t) {
      return t->is<SanitizeTransform>();
    });
  }

  static bool has_sanitizers(const TransformList* MT_NULLABLE transforms) {
    return transforms == nullptr ? false : transforms->has_sanitizers();
  }

  bool sanitizes(const Kind* kind, ApplicationDirection direction) const;

  bool has_source_as_transform() const;

  std::string to_trace_string() const;
  static TransformList from_trace_string(
      const std::string& trace_string,
      Context& context);

  static TransformList from_json(
      const Json::Value& transforms,
      Context& context);

  static TransformList from_kind(const Kind* kind, Context& context);

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
