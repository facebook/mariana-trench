/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

const Transform* TransformsFactory::create_transform(
    const std::string& name) const {
  return transform_.create(name);
}

const TransformList* TransformsFactory::create(
    std::vector<std::string> transforms,
    Context& context) const {
  return transform_lists_.insert(TransformList(transforms, context)).first;
}

const TransformList* MT_NULLABLE TransformsFactory::create(
    TransformList::ConstIterator begin,
    TransformList::ConstIterator end) const {
  if (begin == end) {
    return nullptr;
  }

  return transform_lists_.insert(TransformList(begin, end)).first;
}

const TransformList* TransformsFactory::create(TransformList transforms) const {
  return transform_lists_.insert(transforms).first;
}

const TransformList* TransformsFactory::concat(
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
TransformsFactory::reverse(const TransformList* MT_NULLABLE transforms) const {
  if (transforms == nullptr) {
    return transforms;
  }

  return create(TransformList::reverse_of(transforms));
}

TransformCombinations TransformsFactory::all_combinations(
    const TransformList* transforms) const {
  TransformCombinations combinations{};
  combinations.transform = transforms;

  auto begin = transforms->begin();
  auto end = transforms->end();
  for (auto partition = std::next(begin); partition != end;) {
    combinations.partitions.insert(
        std::make_pair(create(begin, partition), create(partition, end)));
    for (auto inner_end = std::next(partition); inner_end != end;) {
      combinations.subsequences.insert(create(partition, inner_end));
      ++inner_end;
    }
    ++partition;
  }

  return combinations;
}

const TransformsFactory& TransformsFactory::singleton() {
  // Thread-safe global variable, initialized on first call.
  static TransformsFactory instance;
  return instance;
}

} // namespace marianatrench
