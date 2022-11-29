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
#include <PatriciaTreeSet.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Feature.h>

namespace marianatrench {

class FeatureSet final : public sparta::AbstractDomain<FeatureSet> {
 private:
  using Set = sparta::PatriciaTreeSet<const Feature*>;

 public:
  // C++ container concept member types
  using iterator = Set::iterator;
  using const_iterator = iterator;
  using value_type = const Feature*;
  using difference_type = std::ptrdiff_t;
  using size_type = size_t;
  using const_reference = const Feature*&;
  using const_pointer = const Feature**;

 private:
  explicit FeatureSet(Set set) : set_(std::move(set)) {}

 public:
  /* Create the bottom (i.e, empty) feature set. */
  FeatureSet() = default;

  explicit FeatureSet(std::initializer_list<const Feature*> features);

  FeatureSet(const FeatureSet&) = default;
  FeatureSet(FeatureSet&&) = default;
  FeatureSet& operator=(const FeatureSet&) = default;
  FeatureSet& operator=(FeatureSet&&) = default;

  static FeatureSet bottom() {
    return FeatureSet();
  }

  static FeatureSet top() {
    mt_unreachable(); // Not implemented.
  }

  bool is_bottom() const override {
    return set_.empty();
  }

  bool is_top() const override {
    return false;
  }

  void set_to_bottom() override {
    set_.clear();
  }

  void set_to_top() override {
    mt_unreachable(); // Not implemented.
  }

  bool empty() const {
    return set_.empty();
  }

  iterator begin() const {
    return set_.begin();
  }

  iterator end() const {
    return set_.end();
  }

  void add(const Feature* feature);

  void remove(const Feature* feature);

  bool contains(const Feature* feature) const;

  bool leq(const FeatureSet& other) const override;

  bool equals(const FeatureSet& other) const override;

  void join_with(const FeatureSet& other) override;

  void widen_with(const FeatureSet& other) override;

  void meet_with(const FeatureSet& other) override;

  void narrow_with(const FeatureSet& other) override;

  void difference_with(const FeatureSet& other);

  static FeatureSet from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const FeatureSet& features);

  friend class FeatureMayAlwaysSet;

 public:
  Set set_;
};

} // namespace marianatrench
