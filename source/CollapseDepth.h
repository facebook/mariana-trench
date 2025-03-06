/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/ScalarAbstractDomain.h>

namespace marianatrench {

/* The collapse depth for a given output path. */
class CollapseDepth final : public sparta::AbstractDomain<CollapseDepth> {
 private:
  using ScalarAbstractDomain =
      ScalarAbstractDomainScaffolding<scalar_impl::ScalarTopIsZero>;

 public:
  using IntType = ScalarAbstractDomain::IntType;

  enum class Enum : IntType {
    AlwaysCollapse = static_cast<IntType>(ScalarAbstractDomain::Enum::Zero),
    NoCollapse = static_cast<IntType>(ScalarAbstractDomain::Enum::Max),
    Zero = static_cast<IntType>(ScalarAbstractDomain::Enum::Zero),
    Bottom = static_cast<IntType>(ScalarAbstractDomain::Enum::Bottom),
  };

 private:
  explicit CollapseDepth(ScalarAbstractDomain scalar)
      : scalar_(std::move(scalar)) {}

 public:
  /* Create the bottom element. */
  CollapseDepth() : scalar_(ScalarAbstractDomain::bottom()) {}

  explicit CollapseDepth(IntType depth) : scalar_(depth) {}

  explicit CollapseDepth(Enum depth) : scalar_(static_cast<IntType>(depth)) {}

  IntType value() const {
    return scalar_.value();
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CollapseDepth)

  INCLUDE_ABSTRACT_DOMAIN_METHODS(CollapseDepth, ScalarAbstractDomain, scalar_)

  void difference_with(const CollapseDepth& other) {
    scalar_.difference_with(other.scalar_);
  }

  // Note that `top` and `zero` are the same.

  static CollapseDepth zero() {
    return CollapseDepth(Enum::Zero);
  }

  bool is(Enum depth) const {
    return scalar_.value() == static_cast<IntType>(depth);
  }

  bool is_zero() const {
    return this->is(Enum::Zero);
  }

  static CollapseDepth no_collapse() {
    return CollapseDepth(Enum::NoCollapse);
  }

  static CollapseDepth collapse() {
    return CollapseDepth(Enum::AlwaysCollapse);
  }

  bool should_collapse() const {
    return value() < static_cast<IntType>(Enum::NoCollapse);
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const CollapseDepth& depth) {
    if (depth.is(CollapseDepth::Enum::NoCollapse)) {
      return out << "no-collapse";
    }
    return out << depth.scalar_;
  }

 private:
  ScalarAbstractDomain scalar_;
};

} // namespace marianatrench
