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
#include <mariana-trench/Method.h>

namespace marianatrench {

class MethodSet final : public sparta::AbstractDomain<MethodSet> {
 private:
  using Set = sparta::PatriciaTreeSet<const Method*>;

 public:
  // C++ container concept member types
  using iterator = Set::iterator;
  using const_iterator = iterator;
  using value_type = const Method*;
  using difference_type = std::ptrdiff_t;
  using size_type = size_t;
  using const_reference = const Method*&;
  using const_pointer = const Method**;

 private:
  explicit MethodSet(Set set) : set_(std::move(set)) {}

 public:
  /* Create the bottom (i.e, empty) method set. */
  MethodSet() = default;

  explicit MethodSet(std::initializer_list<const Method*> methods);

  MethodSet(const MethodSet&) = default;
  MethodSet(MethodSet&&) = default;
  MethodSet& operator=(const MethodSet&) = default;
  MethodSet& operator=(MethodSet&&) = default;

  static MethodSet bottom() {
    return MethodSet();
  }

  static MethodSet top() {
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

  void add(const Method* method);

  void remove(const Method* method);

  bool leq(const MethodSet& other) const override;

  bool equals(const MethodSet& other) const override;

  void join_with(const MethodSet& other) override;

  void widen_with(const MethodSet& other) override;

  void meet_with(const MethodSet& other) override;

  void narrow_with(const MethodSet& other) override;

  static MethodSet from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  friend std::ostream& operator<<(std::ostream& out, const MethodSet& methods);

 public:
  Set set_;
};

} // namespace marianatrench