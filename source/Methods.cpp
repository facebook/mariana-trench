/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdexcept>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <Walkers.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Redex.h>

namespace marianatrench {

Methods::Methods() = default;

Methods::Methods(const DexStoresVector& stores) {
  // Create all methods with no type overrides.
  for (auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::methods(scope, [&](DexMethod* method) {
      set_.insert(Method(method, /* parameter_type_overrides */ {}));
    });
  }
}

const Method* Methods::create(
    const DexMethod* method,
    ParameterTypeOverrides parameter_type_overrides) {
  mt_assert(method != nullptr);
  return set_.insert(Method(method, parameter_type_overrides)).first;
}

const Method* Methods::get(
    const DexMethod* method,
    ParameterTypeOverrides parameter_type_overrides) const {
  mt_assert(method != nullptr);
  const auto* pointer = set_.get(Method(method, parameter_type_overrides));
  if (!pointer) {
    throw std::logic_error(fmt::format(
        "Method `{}` does not exist in the context",
        Method(method, parameter_type_overrides)));
  }
  return pointer;
}

const Method* MT_NULLABLE Methods::get(const std::string& name) const {
  auto method = redex::get_method(name);
  if (!method) {
    return nullptr;
  }
  return set_.get(Method(method, /* parameter_type_overrides */ {}));
}

Methods::Iterator Methods::begin() const {
  return boost::make_transform_iterator(set_.cbegin(), GetPointer());
}

Methods::Iterator Methods::end() const {
  return boost::make_transform_iterator(set_.cend(), GetPointer());
}

std::size_t Methods::size() const {
  return set_.size();
}

} // namespace marianatrench
