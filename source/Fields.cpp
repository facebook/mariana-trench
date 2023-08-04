/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <DexClass.h>
#include <DexStore.h>
#include <Walkers.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Fields.h>

namespace marianatrench {

Fields::Fields(const DexStoresVector& stores) {
  for (auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::fields(
        scope, [this](DexField* field) { set_.insert(Field(field)); });
  }
}

const Field* Fields::get(const DexField* field) const {
  mt_assert(field != nullptr);
  const auto* pointer = set_.get(Field(field));
  if (!pointer) {
    throw std::logic_error(
        fmt::format("Field `{}` does not exist in the context", Field(field)));
  }
  return pointer;
}

Fields::Iterator Fields::begin() const {
  return boost::make_transform_iterator(set_.cbegin(), GetPointer());
}

Fields::Iterator Fields::end() const {
  return boost::make_transform_iterator(set_.cend(), GetPointer());
}

std::size_t Fields::size() const {
  return set_.size();
}

} // namespace marianatrench
