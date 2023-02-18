/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <PatriciaTreeSetAbstractDomain.h>

#include <mariana-trench/Assert.h>

namespace marianatrench {

template <typename Element, bool bottom_is_empty, bool with_top>
class PatriciaTreeSetAbstractDomain {};

template <typename Element>
class PatriciaTreeSetAbstractDomain<
    Element,
    /* bottom_is_empty */ false,
    /* with_top */ true> : sparta::PatriciaTreeSetAbstractDomain<Element> {};

/**
 * A patricia tree set with an abstract domain structure.
 *
 * The bottom element is represented with the empty set,
 * and the top element is a special value.
 */
template <typename Element>
class PatriciaTreeSetAbstractDomain<
    Element,
    /* bottom_is_empty */ true,
    /* with_top */ true>
    final : public sparta::AbstractDomain<
                PatriciaTreeSetAbstractDomain<Element, true, true>> {
 private:
  using Set = sparta::PatriciaTreeSet<Element>;

 public:
  // C++ container concept member types
  using iterator = typename Set::iterator;
  using const_iterator = iterator;
  using value_type = Element;
  using difference_type = std::ptrdiff_t;
  using size_type = size_t;
  using const_reference = const Element&;
  using const_pointer = const Element*;

 private:
  explicit PatriciaTreeSetAbstractDomain(bool is_top) : is_top_(is_top) {}

 public:
  /* Create the bottom (i.e, empty) set. */
  PatriciaTreeSetAbstractDomain() = default;

  explicit PatriciaTreeSetAbstractDomain(Set set) : set_(std::move(set)) {}

  explicit PatriciaTreeSetAbstractDomain(
      std::initializer_list<Element> elements)
      : set_(elements) {}

  template <typename InputIterator>
  PatriciaTreeSetAbstractDomain(InputIterator first, InputIterator last)
      : set_(first, last) {}

  PatriciaTreeSetAbstractDomain(const PatriciaTreeSetAbstractDomain&) = default;
  PatriciaTreeSetAbstractDomain(PatriciaTreeSetAbstractDomain&&) = default;
  PatriciaTreeSetAbstractDomain& operator=(
      const PatriciaTreeSetAbstractDomain&) = default;
  PatriciaTreeSetAbstractDomain& operator=(PatriciaTreeSetAbstractDomain&&) =
      default;
  ~PatriciaTreeSetAbstractDomain() = default;

  static PatriciaTreeSetAbstractDomain bottom() {
    return PatriciaTreeSetAbstractDomain(/* is_top */ false);
  }

  static PatriciaTreeSetAbstractDomain top() {
    return PatriciaTreeSetAbstractDomain(/* is_top */ true);
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
    is_top_ = true;
    set_.clear();
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

  const Set& elements() const {
    mt_assert(!is_top_);
    return set_;
  }

  size_t size() const {
    mt_assert(!is_top_);
    return set_.size();
  }

  void add(Element element) {
    if (is_top_) {
      return;
    }
    set_.insert(element);
  }

  void remove(Element element) {
    if (is_top_) {
      return;
    }
    set_.remove(element);
  }

  bool contains(Element element) const {
    if (is_top_) {
      return true;
    } else {
      return set_.contains(element);
    }
  }

  bool leq(const PatriciaTreeSetAbstractDomain& other) const override {
    if (is_top_) {
      return other.is_top_;
    }
    if (other.is_top_) {
      return true;
    }
    return set_.is_subset_of(other.set_);
  }

  bool equals(const PatriciaTreeSetAbstractDomain& other) const override {
    return is_top_ == other.is_top_ && set_.equals(other.set_);
  }

  void join_with(const PatriciaTreeSetAbstractDomain& other) override {
    if (is_top_) {
      return;
    }
    if (other.is_top_) {
      set_to_top();
      return;
    }
    set_.union_with(other.set_);
  }

  void widen_with(const PatriciaTreeSetAbstractDomain& other) override {
    join_with(other);
  }

  void meet_with(const PatriciaTreeSetAbstractDomain& other) override {
    if (is_top_) {
      *this = other;
      return;
    }
    if (other.is_top_) {
      return;
    }
    set_.intersection_with(other.set_);
  }

  void narrow_with(const PatriciaTreeSetAbstractDomain& other) override {
    meet_with(other);
  }

  void difference_with(const PatriciaTreeSetAbstractDomain& other) {
    if (other.is_top_) {
      set_to_bottom();
      return;
    }
    if (is_top_) {
      return;
    }
    set_.difference_with(other.set_);
  }

 private:
  Set set_;
  bool is_top_ = false;
};

/**
 * A patricia tree set with an abstract domain structure.
 *
 * The bottom element is represented with the empty set,
 * and the top element does NOT exist.
 *
 * Technically, this is not an abstract domain in the strict definition (since
 * there is no Top element), but this is useful in places where we need the
 * abstract domain interface and know that we will never need a top element.
 */
template <typename Element>
class PatriciaTreeSetAbstractDomain<
    Element,
    /* bottom_is_empty */ true,
    /* with_top */ false>
    final : public sparta::AbstractDomain<
                PatriciaTreeSetAbstractDomain<Element, true, false>> {
 private:
  using Set = sparta::PatriciaTreeSet<Element>;

 public:
  // C++ container concept member types
  using iterator = typename Set::iterator;
  using const_iterator = iterator;
  using value_type = Element;
  using difference_type = std::ptrdiff_t;
  using size_type = size_t;
  using const_reference = const Element&;
  using const_pointer = const Element*;

 public:
  /* Create the bottom (i.e, empty) set. */
  PatriciaTreeSetAbstractDomain() = default;

  explicit PatriciaTreeSetAbstractDomain(Set set) : set_(std::move(set)) {}

  explicit PatriciaTreeSetAbstractDomain(
      std::initializer_list<Element> elements)
      : set_(elements) {}

  template <typename InputIterator>
  PatriciaTreeSetAbstractDomain(InputIterator first, InputIterator last)
      : set_(first, last) {}

  PatriciaTreeSetAbstractDomain(const PatriciaTreeSetAbstractDomain&) = default;
  PatriciaTreeSetAbstractDomain(PatriciaTreeSetAbstractDomain&&) = default;
  PatriciaTreeSetAbstractDomain& operator=(
      const PatriciaTreeSetAbstractDomain&) = default;
  PatriciaTreeSetAbstractDomain& operator=(PatriciaTreeSetAbstractDomain&&) =
      default;
  ~PatriciaTreeSetAbstractDomain() = default;

  static PatriciaTreeSetAbstractDomain bottom() {
    return PatriciaTreeSetAbstractDomain();
  }

  static PatriciaTreeSetAbstractDomain top() {
    mt_unreachable(); // does not exist
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
    mt_unreachable();
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

  const Set& elements() const {
    return set_;
  }

  size_t size() const {
    return set_.size();
  }

  void add(Element element) {
    set_.insert(element);
  }

  void remove(Element element) {
    set_.remove(element);
  }

  bool contains(Element element) const {
    return set_.contains(element);
  }

  bool leq(const PatriciaTreeSetAbstractDomain& other) const override {
    return set_.is_subset_of(other.set_);
  }

  bool equals(const PatriciaTreeSetAbstractDomain& other) const override {
    return set_.equals(other.set_);
  }

  void join_with(const PatriciaTreeSetAbstractDomain& other) override {
    set_.union_with(other.set_);
  }

  void widen_with(const PatriciaTreeSetAbstractDomain& other) override {
    join_with(other);
  }

  void meet_with(const PatriciaTreeSetAbstractDomain& other) override {
    set_.intersection_with(other.set_);
  }

  void narrow_with(const PatriciaTreeSetAbstractDomain& other) override {
    meet_with(other);
  }

  void difference_with(const PatriciaTreeSetAbstractDomain& other) {
    set_.difference_with(other.set_);
  }

 private:
  Set set_;
};

} // namespace marianatrench
