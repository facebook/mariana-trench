/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/PatriciaTreeSetAbstractDomain.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/IncludeMacros.h>

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

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(
      PatriciaTreeSetAbstractDomain)

  static PatriciaTreeSetAbstractDomain bottom() {
    return PatriciaTreeSetAbstractDomain(/* is_top */ false);
  }

  static PatriciaTreeSetAbstractDomain top() {
    return PatriciaTreeSetAbstractDomain(/* is_top */ true);
  }

  bool is_bottom() const {
    return !is_top_ && set_.empty();
  }

  bool is_top() const {
    return is_top_;
  }

  void set_to_bottom() {
    is_top_ = false;
    set_.clear();
  }

  void set_to_top() {
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

  const Element* MT_NULLABLE singleton() const {
    if (is_top_) {
      return nullptr;
    } else {
      return set_.singleton();
    }
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

  bool leq(const PatriciaTreeSetAbstractDomain& other) const {
    if (is_top_) {
      return other.is_top_;
    }
    if (other.is_top_) {
      return true;
    }
    return set_.is_subset_of(other.set_);
  }

  bool equals(const PatriciaTreeSetAbstractDomain& other) const {
    return is_top_ == other.is_top_ && set_.equals(other.set_);
  }

  void join_with(const PatriciaTreeSetAbstractDomain& other) {
    if (is_top_) {
      return;
    }
    if (other.is_top_) {
      set_to_top();
      return;
    }
    set_.union_with(other.set_);
  }

  void widen_with(const PatriciaTreeSetAbstractDomain& other) {
    join_with(other);
  }

  void meet_with(const PatriciaTreeSetAbstractDomain& other) {
    if (is_top_) {
      *this = other;
      return;
    }
    if (other.is_top_) {
      return;
    }
    set_.intersection_with(other.set_);
  }

  void narrow_with(const PatriciaTreeSetAbstractDomain& other) {
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

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(
      PatriciaTreeSetAbstractDomain)

  static PatriciaTreeSetAbstractDomain bottom() {
    return PatriciaTreeSetAbstractDomain();
  }

  static PatriciaTreeSetAbstractDomain top() {
    mt_unreachable(); // does not exist
  }

  bool is_bottom() const {
    return set_.empty();
  }

  bool is_top() const {
    return false;
  }

  void set_to_bottom() {
    set_.clear();
  }

  void set_to_top() {
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

  const Element* MT_NULLABLE singleton() const {
    return set_.singleton();
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

  bool leq(const PatriciaTreeSetAbstractDomain& other) const {
    return set_.is_subset_of(other.set_);
  }

  bool equals(const PatriciaTreeSetAbstractDomain& other) const {
    return set_.equals(other.set_);
  }

  void join_with(const PatriciaTreeSetAbstractDomain& other) {
    set_.union_with(other.set_);
  }

  void widen_with(const PatriciaTreeSetAbstractDomain& other) {
    join_with(other);
  }

  void meet_with(const PatriciaTreeSetAbstractDomain& other) {
    set_.intersection_with(other.set_);
  }

  void narrow_with(const PatriciaTreeSetAbstractDomain& other) {
    meet_with(other);
  }

  void difference_with(const PatriciaTreeSetAbstractDomain& other) {
    set_.difference_with(other.set_);
  }

 private:
  Set set_;
};

} // namespace marianatrench
