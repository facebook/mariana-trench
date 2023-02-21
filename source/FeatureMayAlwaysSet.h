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
#include <PatriciaTreeOverUnderSetAbstractDomain.h>
#include <PatriciaTreeSet.h>

#include <mariana-trench/Feature.h>
#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

/**
 * Represents the sets of may and always features.
 * May-features are features that happen on at least one of the flows.
 * Always-features are features that happen on all flows.
 */
class FeatureMayAlwaysSet final
    : public sparta::AbstractDomain<FeatureMayAlwaysSet> {
 private:
  using OverUnderSet =
      sparta::PatriciaTreeOverUnderSetAbstractDomain<const Feature*>;

 private:
  explicit FeatureMayAlwaysSet(OverUnderSet set) : set_(std::move(set)) {}

 public:
  /* Create the empty may-always feature set. */
  FeatureMayAlwaysSet() = default;

  /* Create the may-always feature set with the given always-features. */
  explicit FeatureMayAlwaysSet(std::initializer_list<const Feature*> features);

  FeatureMayAlwaysSet(const FeatureSet& may, const FeatureSet& always);

  static FeatureMayAlwaysSet make_may(
      std::initializer_list<const Feature*> features);

  static FeatureMayAlwaysSet make_may(const FeatureSet& features);

  static FeatureMayAlwaysSet make_always(
      std::initializer_list<const Feature*> features);

  static FeatureMayAlwaysSet make_always(const FeatureSet& features);

  FeatureMayAlwaysSet(const FeatureMayAlwaysSet&) = default;
  FeatureMayAlwaysSet(FeatureMayAlwaysSet&&) = default;
  FeatureMayAlwaysSet& operator=(const FeatureMayAlwaysSet&) = default;
  FeatureMayAlwaysSet& operator=(FeatureMayAlwaysSet&&) = default;

  INCLUDE_ABSTRACT_DOMAIN_METHODS(FeatureMayAlwaysSet, OverUnderSet, set_)

  /* Return true if this is neither top nor bottom. */
  bool is_value() const {
    return set_.is_value();
  }

  bool empty() const {
    return set_.empty();
  }

  FeatureSet may() const;

  FeatureSet always() const;

  void add_may(const Feature* feature);

  void add_may(const FeatureSet& features);

  void add_always(const Feature* feature);

  void add_always(const FeatureSet& feature);

  void add(const FeatureMayAlwaysSet& other);

  static FeatureMayAlwaysSet from_json(
      const Json::Value& value,
      Context& context);
  Json::Value to_json() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const FeatureMayAlwaysSet& features);

 private:
  OverUnderSet set_;
};

} // namespace marianatrench
