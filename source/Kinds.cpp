/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/Kinds.h>
#include <mariana-trench/NamedKind.h>
#include <mariana-trench/PartialKind.h>
#include <mariana-trench/TriggeredPartialKind.h>

namespace marianatrench {

const NamedKind* Kinds::get(const std::string& name) const {
  return named_.create(name);
}

const PartialKind* Kinds::get_partial(
    const std::string& name,
    const std::string& label) const {
  return partial_.create(std::make_pair(name, label), name, label);
}

const TriggeredPartialKind* Kinds::get_triggered(
    const PartialKind* partial) const {
  return triggered_partial_.create(partial);
}

std::vector<const Kind*> Kinds::kinds() const {
  std::vector<const Kind*> result;
  for (const auto& [_key, kind] : named_) {
    result.push_back(kind);
  }
  for (const auto& [_key, kind] : partial_) {
    result.push_back(kind);
  }
  for (const auto& [_key, kind] : triggered_partial_) {
    result.push_back(kind);
  }

  return result;
}

const Kind* Kinds::artificial_source() {
  static const NamedKind kind("<ArtificialSource>");
  return &kind;
}

} // namespace marianatrench
