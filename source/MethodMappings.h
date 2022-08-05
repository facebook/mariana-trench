/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <HashedSetAbstractDomain.h>

#include <mariana-trench/Methods.h>

namespace marianatrench {

using MethodHashedSet = sparta::HashedSetAbstractDomain<const Method*>;

class MethodMappings {
 public:
  explicit MethodMappings(const Methods& methods);

  MethodMappings(const MethodMappings&) = delete;
  MethodMappings(MethodMappings&&) = delete;
  MethodMappings& operator=(MethodMappings&&) = delete;
  MethodMappings& operator=(const MethodMappings&) = delete;
  ~MethodMappings() = default;

 public:
  const ConcurrentMap<std::string_view, MethodHashedSet>& name_to_methods()
      const {
    return name_to_methods_;
  }

  const ConcurrentMap<std::string_view, MethodHashedSet>& class_to_methods()
      const {
    return class_to_methods_;
  }

  const ConcurrentMap<std::string_view, MethodHashedSet>&
  class_to_override_methods() const {
    return class_to_override_methods_;
  }

  const ConcurrentMap<std::string, MethodHashedSet>& signature_to_methods()
      const {
    return signature_to_methods_;
  }

  const MethodHashedSet& all_methods() const {
    return all_methods_;
  }

 private:
  ConcurrentMap<std::string_view, MethodHashedSet> name_to_methods_;
  ConcurrentMap<std::string_view, MethodHashedSet> class_to_methods_;
  ConcurrentMap<std::string_view, MethodHashedSet> class_to_override_methods_;
  ConcurrentMap<std::string, MethodHashedSet> signature_to_methods_;
  MethodHashedSet all_methods_;
};

} // namespace marianatrench
