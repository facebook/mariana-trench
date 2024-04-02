/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/AbstractDomain.h>

#include <mariana-trench/Origin.h>
#include <mariana-trench/PatriciaTreeSetAbstractDomain.h>

namespace marianatrench {

class OriginSet final : public sparta::AbstractDomain<OriginSet> {
 private:
  using Set = PatriciaTreeSetAbstractDomain<
      const Origin*,
      /* bottom_is_empty */ true,
      /* with_top */ false>;

 public:
  INCLUDE_SET_MEMBER_TYPES(Set, const Origin*)

 private:
  explicit OriginSet(Set set) : set_(std::move(set)) {}

 public:
  /* Create the bottom (i.e, empty) method set. */
  OriginSet() = default;

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(OriginSet)
  INCLUDE_ABSTRACT_DOMAIN_METHODS(OriginSet, Set, set_)
  INCLUDE_SET_METHODS(OriginSet, Set, set_, const Origin*, iterator)

  static OriginSet from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  friend std::ostream& operator<<(std::ostream& out, const OriginSet& methods);

 private:
  Set set_;
};

} // namespace marianatrench
