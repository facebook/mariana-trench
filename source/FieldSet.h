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
#include <mariana-trench/Field.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/PatriciaTreeSetAbstractDomain.h>

namespace marianatrench {

class FieldSet final : public sparta::AbstractDomain<FieldSet> {
 private:
  using Set = PatriciaTreeSetAbstractDomain<
      const Field*,
      /* bottom_is_empty */ true,
      /* with_top */ true>;

 public:
  INCLUDE_SET_MEMBER_TYPES(Set, const Field*)

 private:
  explicit FieldSet(Set set) : set_(std::move(set)) {}

 public:
  /* Create the bottom (i.e, empty) field set. */
  FieldSet() = default;

  explicit FieldSet(const Fields& fields);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(FieldSet)
  INCLUDE_ABSTRACT_DOMAIN_METHODS(FieldSet, Set, set_)
  INCLUDE_SET_METHODS(FieldSet, Set, set_, const Field*, iterator)

  static FieldSet from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  friend std::ostream& operator<<(std::ostream& out, const FieldSet& fields);

 private:
  Set set_;
};

} // namespace marianatrench
