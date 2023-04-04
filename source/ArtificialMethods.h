/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Model.h>

namespace marianatrench {

/**
 * This class adds artificial methods that serves as artifical sources or sinks
 * in the analysis.
 */
class ArtificialMethods final {
 public:
  explicit ArtificialMethods(
      const KindFactory& kind_factory,
      DexStoresVector& stores);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ArtificialMethods)

  /* Models for artificial methods. */
  std::vector<Model> models(Context& context) const;

  /* An artificial method called on array allocations with a size parameter. */
  DexMethod* array_allocation_method() const {
    // If the kind is unused, attempting to access the method is likely
    // unintentional. Check `array_allocation_kind_used()` first.
    mt_assert(array_allocation_kind_used_);
    return array_allocation_method_;
  }

  /* Underlying kind associated with the `array_allocation_method()`. */
  const Kind* array_allocation_kind() const {
    // If the kind is unused, attempting to access the kind is likely
    // unintentional. Check `array_allocation_kind_used()` first.
    mt_assert(array_allocation_kind_used_);
    return array_allocation_kind_;
  }

  bool array_allocation_kind_used() const {
    return array_allocation_kind_used_;
  }

  /* Marks the given set of kinds as unused. Kinds that do not pertain to
   * artificial methods are ignored. */
  void set_unused_kinds(const std::unordered_set<const Kind*>& unused_kinds);

 private:
  DexMethod* array_allocation_method_;
  const Kind* array_allocation_kind_;
  bool array_allocation_kind_used_;
};

} // namespace marianatrench
