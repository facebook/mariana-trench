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

#include <AbstractDomain.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Feature.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/PatriciaTreeSetAbstractDomain.h>

namespace marianatrench {

class FeatureSet final : public sparta::AbstractDomain<FeatureSet> {
 private:
  using Set = PatriciaTreeSetAbstractDomain<
      const Feature*,
      /* bottom_is_empty */ true,
      /* with_top */ false>;

 public:
  INCLUDE_SET_MEMBER_TYPES(Set, const Feature*)

 private:
  explicit FeatureSet(Set set) : set_(std::move(set)) {}

 public:
  /* Create the bottom (i.e, empty) feature set. */
  FeatureSet() = default;

  FeatureSet(const FeatureSet&) = default;
  FeatureSet(FeatureSet&&) = default;
  FeatureSet& operator=(const FeatureSet&) = default;
  FeatureSet& operator=(FeatureSet&&) = default;
  ~FeatureSet() = default;

  INCLUDE_ABSTRACT_DOMAIN_METHODS(FeatureSet, Set, set_)
  INCLUDE_SET_METHODS(FeatureSet, Set, set_, const Feature*, iterator)

  static FeatureSet from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const FeatureSet& features);

 public:
  Set set_;
};

} // namespace marianatrench
