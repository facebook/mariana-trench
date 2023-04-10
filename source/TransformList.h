/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

/**
 * Represents a single Transform operation applied on a taint kind.
 */
class Transform final {
 public:
  explicit Transform(std::string name) : name_(std::move(name)) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Transform)

  static const Transform* from_json(const Json::Value& value, Context& context);
  std::string to_trace_string() const;

  const auto& name() const {
    return name_;
  }

 private:
  std::string name_;
};

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

  std::string to_trace_string() const;

  static TransformList from_json(
      const Json::Value& transforms,
      Context& context);

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
