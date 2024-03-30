/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/HashedSetAbstractDomain.h>

#include <mariana-trench/Access.h>

namespace marianatrench {

/**
 * Represents a (root, tag) pair which allows associating a string tag to the
 * root. Used with via-value/type-of ports to allow associating user-defined
 * tags with the materialized feature.
 */
class TaggedRoot final {
 public:
  explicit TaggedRoot(Root root, const DexString* MT_NULLABLE tag)
      : root_(std::move(root)), tag_(tag) {}

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(TaggedRoot)

  bool operator==(const TaggedRoot& other) const {
    return root_ == other.root_ && tag_ == other.tag_;
  }

  bool operator!=(const TaggedRoot& other) const {
    return !(*this == other);
  }

  const Root& root() const {
    return root_;
  }

  const DexString* MT_NULLABLE tag() const {
    return tag_;
  }

  static TaggedRoot from_json(const Json::Value& value);
  Json::Value to_json() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const TaggedRoot& tagged_root);

 private:
  Root root_;
  const DexString* MT_NULLABLE tag_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::TaggedRoot> {
  std::size_t operator()(const marianatrench::TaggedRoot& tagged_root) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, tagged_root.root().hash());
    boost::hash_combine(seed, tagged_root.tag());
    return seed;
  }
};

namespace marianatrench {

using TaggedRootSet = sparta::HashedSetAbstractDomain<TaggedRoot>;

} // namespace marianatrench
