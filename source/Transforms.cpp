/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/Transforms.h>

namespace marianatrench {

const TransformList* Transforms::create(
    std::vector<std::string> transforms,
    Context& context) const {
  return transform_lists_.insert(TransformList(transforms, context)).first;
}

const TransformList* MT_NULLABLE Transforms::create(
    TransformList::ConstIterator begin,
    TransformList::ConstIterator end) const {
  if (begin == end) {
    return nullptr;
  }

  return transform_lists_.insert(TransformList(begin, end)).first;
}

const TransformList* Transforms::create(TransformList transforms) const {
  return transform_lists_.insert(transforms).first;
}

const TransformList* Transforms::concat(
    const TransformList* MT_NULLABLE left,
    const TransformList* MT_NULLABLE right) const {
  if (left == nullptr) {
    mt_assert(right != nullptr);
    return right;
  } else if (right == nullptr) {
    mt_assert(left != nullptr);
    return left;
  }

  TransformList::List transforms{};
  transforms.reserve(left->size() + right->size());
  transforms.insert(transforms.end(), left->begin(), left->end());
  transforms.insert(transforms.end(), right->begin(), right->end());

  return create(TransformList(transforms));
}

const TransformList* MT_NULLABLE
Transforms::reverse(const TransformList* MT_NULLABLE transforms) const {
  if (transforms == nullptr) {
    return transforms;
  }

  return create(TransformList::reverse_of(transforms));
}

} // namespace marianatrench
