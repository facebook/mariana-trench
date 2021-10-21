/*
 * Copyright (c) Facebook, Inc. and its affiliates.
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
#include <mariana-trench/Field.h>

namespace marianatrench {

class FieldSet final : public sparta::AbstractDomain<FieldSet> {
 private:
  using Set = sparta::PatriciaTreeSet<const Field*>;

 public:
  // C++ container concept member types
  using iterator = Set::iterator;
  using const_iterator = iterator;
  using value_type = const Field*;
  using difference_type = std::ptrdiff_t;
  using size_type = size_t;
  using const_reference = const Field*&;
  using const_pointer = const Field**;

 private:
  explicit FieldSet(Set set) : set_(std::move(set)) {}

 public:
  /* Create the bottom (i.e, empty) field set. */
  FieldSet() = default;

  explicit FieldSet(std::initializer_list<const Field*> fields);
  explicit FieldSet(const Fields& fields);

  FieldSet(const FieldSet&) = default;
  FieldSet(FieldSet&&) = default;
  FieldSet& operator=(const FieldSet&) = default;
  FieldSet& operator=(FieldSet&&) = default;

  static FieldSet bottom() {
    return FieldSet();
  }

  static FieldSet top() {
    auto field_set = FieldSet();
    field_set.set_to_top();
    return field_set;
  }

  bool is_bottom() const override {
    return !is_top_ && set_.empty();
  }

  bool is_top() const override {
    return is_top_;
  }

  void set_to_bottom() override {
    is_top_ = false;
    set_.clear();
  }

  void set_to_top() override {
    set_.clear();
    is_top_ = true;
  }

  bool empty() const {
    return is_bottom();
  }

  iterator begin() const {
    mt_assert(!is_top_);
    return set_.begin();
  }

  iterator end() const {
    mt_assert(!is_top_);
    return set_.end();
  }

  void add(const Field* field);

  void remove(const Field* field);

  bool leq(const FieldSet& other) const override;

  bool equals(const FieldSet& other) const override;

  void join_with(const FieldSet& other) override;

  void widen_with(const FieldSet& other) override;

  void meet_with(const FieldSet& other) override;

  void narrow_with(const FieldSet& other) override;

  void difference_with(const FieldSet& other);

  static FieldSet from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  friend std::ostream& operator<<(std::ostream& out, const FieldSet& fields);

 private:
  Set set_;
  bool is_top_ = false;
};

} // namespace marianatrench
