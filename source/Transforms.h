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

namespace marianatrench {

class Transforms final {
 public:
  Transforms() = default;

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Transforms)

 public:
  size_t size() const {
    return transform_lists_.size();
  }

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

 private:
  mutable InsertOnlyConcurrentSet<TransformList> transform_lists_;
};

} // namespace marianatrench
