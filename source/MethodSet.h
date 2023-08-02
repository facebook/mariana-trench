/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <initializer_list>
#include <ostream>

#include <json/json.h>

#include <sparta/AbstractDomain.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/PatriciaTreeSetAbstractDomain.h>

namespace marianatrench {

class MethodSet final : public sparta::AbstractDomain<MethodSet> {
 private:
  using Set = PatriciaTreeSetAbstractDomain<
      const Method*,
      /* bottom_is_empty */ true,
      /* with_top */ true>;

 public:
  INCLUDE_SET_MEMBER_TYPES(Set, const Method*)

 private:
  explicit MethodSet(Set set) : set_(std::move(set)) {}

 public:
  /* Create the bottom (i.e, empty) method set. */
  MethodSet() = default;

  explicit MethodSet(const Methods& methods);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(MethodSet)
  INCLUDE_ABSTRACT_DOMAIN_METHODS(MethodSet, Set, set_)
  INCLUDE_SET_METHODS(MethodSet, Set, set_, const Method*, iterator)

  static MethodSet from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  friend std::ostream& operator<<(std::ostream& out, const MethodSet& methods);

 private:
  Set set_;
};

} // namespace marianatrench
