// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <vector>

#include <mariana-trench/Context.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Model.h>

namespace marianatrench {

/**
 * This class adds artificial methods that serves as artifical sources or sinks
 * in the analysis.
 */
class ArtificialMethods final {
 public:
  explicit ArtificialMethods(Kinds& kinds, DexStoresVector& stores);

  ArtificialMethods(const ArtificialMethods&) = delete;
  ArtificialMethods(ArtificialMethods&&) = delete;
  ArtificialMethods& operator=(const ArtificialMethods&) = delete;
  ArtificialMethods& operator=(ArtificialMethods&&) = delete;
  ~ArtificialMethods() = default;

  /* Models for artificial methods. */
  std::vector<Model> models(Context& context) const;

  /* An artificial method called on array allocations with a size parameter. */
  DexMethod* array_allocation_method() const {
    return array_allocation_method_;
  }

  const Kind* array_allocation_kind() const {
    return array_allocation_kind_;
  }

 private:
  DexMethod* array_allocation_method_;
  const Kind* array_allocation_kind_;
};

} // namespace marianatrench
