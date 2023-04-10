/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ConcurrentContainers.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/TransformList.h>
#include <mariana-trench/UniquePointerFactory.h>

namespace marianatrench {

struct TransformCombinations {
  const TransformList* transform;
  std::unordered_set<
      std::pair<const TransformList*, const TransformList*>,
      boost::hash<std::pair<const TransformList*, const TransformList*>>>
      partitions;
  std::unordered_set<const TransformList*> subsequences;
};

class TransformsFactory final {
 public:
  TransformsFactory() = default;

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(TransformsFactory)

 public:
  size_t size() const {
    return transform_lists_.size();
  }

  const Transform* create_transform(const std::string& name) const;

  // Use for testing only
  const TransformList* create(
      std::vector<std::string> transforms,
      Context& context) const;

  const TransformList* create(TransformList transforms) const;

  const TransformList* create(
      TransformList::ConstIterator begin,
      TransformList::ConstIterator end) const;

  const TransformList* concat(
      const TransformList* MT_NULLABLE local_transforms,
      const TransformList* MT_NULLABLE global_transforms) const;

  const TransformList* MT_NULLABLE
  reverse(const TransformList* MT_NULLABLE transforms) const;

  TransformCombinations all_combinations(const TransformList* transforms) const;

  static const TransformsFactory& singleton();

 private:
  UniquePointerFactory<std::string, Transform> transform_;
  mutable InsertOnlyConcurrentSet<TransformList> transform_lists_;
};

} // namespace marianatrench
