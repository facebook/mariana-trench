/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ostream>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include <boost/iterator/transform_iterator.hpp>

#include <AbstractDomain.h>

#include <mariana-trench/Assert.h>

namespace marianatrench {

namespace detail {

/**
 * This is a wrapper around a mutable value. It allows getting a mutable
 * reference on a constant instance. This is a type safe alternative to
 * `const_cast`, since mutating a reference produced by `const_cast` is
 * undefined behavior.
 */
template <typename Value>
class MutableValue {
 public:
  explicit MutableValue(Value value) : value_(std::move(value)) {}
  MutableValue(const MutableValue&) = default;
  MutableValue(MutableValue&&) = default;
  MutableValue& operator=(const MutableValue&) = default;
  MutableValue& operator=(MutableValue&&) = default;
  ~MutableValue() = default;

  Value& get() {
    return value_;
  }

  const Value& get() const {
    return value_;
  }

  Value& get_unsafe() const {
    return value_;
  }

 private:
  mutable Value value_;
};

template <typename Element>
struct GroupDifference {
  void operator()(Element& left, const Element& right) const {
    if (left.leq(right)) {
      left.set_to_bottom();
    }
  }
};

} // namespace detail

/**
 * A powerset abstract domain with grouping implemented using hash tables.
 *
 * `GroupHash` and `GroupEqual` describe how elements are grouped together.
 *
 * The implementation is mostly based on `sparta::HashedSetAbstractDomain`.
 */
template <
    typename Element,
    typename GroupHash,
    typename GroupEqual,
    typename GroupDifference = detail::GroupDifference<Element>>
class GroupHashedSetAbstractDomain final
    : public sparta::AbstractDomain<GroupHashedSetAbstractDomain<
          Element,
          GroupHash,
          GroupEqual,
          GroupDifference>> {
 public:
  static_assert(std::is_same_v<
                decltype(GroupHash()(std::declval<const Element>())),
                std::size_t>);
  static_assert(std::is_same_v<
                decltype(GroupEqual()(
                    std::declval<const Element>(),
                    std::declval<const Element>())),
                bool>);
  static_assert(std::is_same_v<
                decltype(GroupDifference()(
                    std::declval<Element&>(),
                    std::declval<const Element>())),
                void>);

 private:
  using MutableElement = detail::MutableValue<Element>;

  struct MutableElementHash {
    std::size_t operator()(const MutableElement& element) const {
      return GroupHash()(element.get());
    }
  };

  struct MutableElementEqual {
    bool operator()(const MutableElement& left, const MutableElement& right)
        const {
      return GroupEqual()(left.get(), right.get());
    }
  };

  struct ExposeElement {
    const Element& operator()(const MutableElement& element) const {
      return element.get();
    }
  };

  using Set = std::
      unordered_set<MutableElement, MutableElementHash, MutableElementEqual>;

  using ConstIterator =
      boost::transform_iterator<ExposeElement, typename Set::const_iterator>;

 public:
  // C++ container concept member types
  using iterator = ConstIterator;
  using const_iterator = ConstIterator;
  using value_type = Element;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const Element&;
  using const_pointer = const Element*;

 public:
  /* Create the bottom (i.e, empty) abstract set. */
  GroupHashedSetAbstractDomain() = default;

  explicit GroupHashedSetAbstractDomain(const Element& element) {
    add(element);
  }

  explicit GroupHashedSetAbstractDomain(
      std::initializer_list<Element> elements) {
    for (const auto& element : elements) {
      add(element);
    }
  }

  GroupHashedSetAbstractDomain(const GroupHashedSetAbstractDomain&) = default;
  GroupHashedSetAbstractDomain(GroupHashedSetAbstractDomain&&) = default;
  GroupHashedSetAbstractDomain& operator=(const GroupHashedSetAbstractDomain&) =
      default;
  GroupHashedSetAbstractDomain& operator=(GroupHashedSetAbstractDomain&&) =
      default;

  static GroupHashedSetAbstractDomain bottom() {
    return GroupHashedSetAbstractDomain();
  }

  static GroupHashedSetAbstractDomain top() {
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

  std::size_t size() const {
    return set_.size();
  }

  bool empty() const {
    return set_.empty();
  }

  ConstIterator begin() const {
    return boost::make_transform_iterator(set_.cbegin(), ExposeElement());
  }

  ConstIterator end() const {
    return boost::make_transform_iterator(set_.cend(), ExposeElement());
  }

  bool contains(const Element& element) const {
    if (element.is_bottom()) {
      return true;
    }

    auto found = set_.find(MutableElement(element));
    return found != set_.end() && element.leq(found->get());
  }

  void add(const Element& element) {
    if (element.is_bottom()) {
      return;
    }

    auto result = set_.emplace(element);
    if (!result.second) {
      // This is safe as long as `join_with` does not change the grouping
      result.first->get_unsafe().join_with(element);
    }
  }

  void remove(const Element& element) {
    if (element.is_bottom()) {
      return;
    }

    auto found = set_.find(MutableElement(element));
    if (found != set_.end() && found->get().leq(element)) {
      set_.erase(found);
    }
  }

  void clear() {
    set_.clear();
  }

  bool leq(const GroupHashedSetAbstractDomain& other) const override {
    if (set_.size() > other.set_.size()) {
      return false;
    }
    for (const MutableElement& mutable_element : set_) {
      const Element& element = mutable_element.get();
      auto found = other.set_.find(mutable_element);
      if (found == other.set_.end() || !element.leq(found->get())) {
        return false;
      }
    }
    return true;
  }

  bool equals(const GroupHashedSetAbstractDomain& other) const override {
    if (set_.size() != other.set_.size()) {
      return false;
    }
    for (const MutableElement& mutable_element : set_) {
      const Element& element = mutable_element.get();
      auto found = other.set_.find(mutable_element);
      if (found == other.set_.end() || !(element == found->get())) {
        return false;
      }
    }
    return true;
  }

  void join_with(const GroupHashedSetAbstractDomain& other) override {
    for (const MutableElement& mutable_element : other.set_) {
      auto result = set_.insert(mutable_element);
      if (!result.second) {
        // This is safe as long as `join_with` does not change the grouping.
        result.first->get_unsafe().join_with(mutable_element.get());
      }
    }
  }

  void widen_with(const GroupHashedSetAbstractDomain& other) override {
    join_with(other);
  }

  void meet_with(const GroupHashedSetAbstractDomain& /*other*/) override {
    mt_unreachable(); // Not implemented.
  }

  void narrow_with(const GroupHashedSetAbstractDomain& other) override {
    meet_with(other);
  }

  void difference_with(const GroupHashedSetAbstractDomain& other) {
    // For performance, we iterate on the smallest set.
    if (set_.size() <= other.set_.size()) {
      for (auto iterator = set_.begin(), end = set_.end(); iterator != end;) {
        auto found = other.set_.find(*iterator);
        if (found != other.set_.end()) {
          GroupDifference()(iterator->get_unsafe(), found->get());
          if (iterator->get().is_bottom()) {
            iterator = set_.erase(iterator);
          } else {
            ++iterator;
          }
        } else {
          ++iterator;
        }
      }
    } else {
      for (const MutableElement& element : other.set_) {
        auto found = set_.find(element);
        if (found != set_.end()) {
          GroupDifference()(found->get_unsafe(), element.get());
          if (found->get().is_bottom()) {
            set_.erase(found);
          }
        }
      }
    }
  }

  /* Update all elements without affecting the grouping. */
  void map(const std::function<void(Element&)>& f) {
    for (auto iterator = set_.begin(), end = set_.end(); iterator != end;) {
      // This is safe as long as `f` does not change the grouping.
      const MutableElement& mutable_element = *iterator;
      auto previous_hash = MutableElementHash()(mutable_element);
      f(mutable_element.get_unsafe());
      if (mutable_element.get().is_bottom()) {
        iterator = set_.erase(iterator);
      } else {
        auto current_hash = MutableElementHash()(mutable_element);
        mt_assert_log(current_hash == previous_hash, "group hash has changed");
        ++iterator;
      }
    }
  }

  /* Remove all elements that do not match the given predicate. */
  void filter(const std::function<bool(const Element&)>& predicate) {
    for (auto iterator = set_.begin(), end = set_.end(); iterator != end;) {
      if (!predicate(iterator->get())) {
        iterator = set_.erase(iterator);
      } else {
        iterator++;
      }
    }
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const GroupHashedSetAbstractDomain& value) {
    out << "{";
    for (auto it = value.begin(); it != value.end();) {
      out << *it++;
      if (it != value.end()) {
        out << ", ";
      }
    }
    return out << "}";
  }

 private:
  Set set_;
};

} // namespace marianatrench
