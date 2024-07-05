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

const NamedTransform* TransformsFactory::create_transform(
    const std::string& name) const {
  return transform_.create(name);
}

const SourceAsTransform* TransformsFactory::create_source_as_transform(
    const Kind* kind) const {
  return source_as_transform_.create(kind);
}

const SanitizerSetTransform* TransformsFactory::create_sanitizer_set_transform(
    const SanitizerSetTransform::Set& kinds) const {
  return sanitize_transform_set_.create(kinds);
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

const TransformList* TransformsFactory::create(
    std::vector<const Transform*> transforms) const {
  mt_assert(!transforms.empty());
  return create(TransformList(std::move(transforms)));
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

  return create(TransformList::concat(left, right));
}

const TransformList* MT_NULLABLE
TransformsFactory::reverse(const TransformList* MT_NULLABLE transforms) const {
  if (transforms == nullptr) {
    return transforms;
  }

  return create(TransformList::reverse_of(transforms));
}

const TransformList* MT_NULLABLE
TransformsFactory::get_source_as_transform(const Kind* kind) const {
  if (const auto* source_as_transform = source_as_transform_.get(kind)) {
    return transform_lists_.get(TransformList({source_as_transform}));
  }

  return nullptr;
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

const TransformList* MT_NULLABLE TransformsFactory::discard_sanitizers(
    const TransformList* MT_NULLABLE transforms) const {
  if (transforms == nullptr) {
    return transforms;
  }

  auto no_sanitizers = TransformList::discard_sanitizers(transforms);

  if (no_sanitizers.size() == 0) {
    return nullptr;
  }

  return create(std::move(no_sanitizers));
}

const TransformList* MT_NULLABLE TransformsFactory::canonicalize(
    const TransformList* MT_NULLABLE transforms) const {
  if (transforms == nullptr) {
    return transforms;
  }

  return create(TransformList::canonicalize(transforms, *this));
}

} // namespace marianatrench
