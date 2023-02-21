/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <iterator>
#include <optional>
#include <type_traits>

#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

template <typename OuterIterator>
struct FlattenDereference {
  using Reference = typename std::iterator_traits<OuterIterator>::reference;
  using InnerIterator = decltype(std::declval<Reference>().begin());

  static InnerIterator begin(Reference reference) {
    return reference.begin();
  }

  static InnerIterator end(Reference reference) {
    return reference.end();
  }
};

template <typename OuterIterator>
struct FlattenConstDereference {
  using Reference = typename std::iterator_traits<OuterIterator>::reference;
  using InnerIterator = decltype(std::declval<Reference>().cbegin());

  static InnerIterator begin(Reference reference) {
    return reference.cbegin();
  }

  static InnerIterator end(Reference reference) {
    return reference.cend();
  }
};

/**
 * A flattening iterator that iterates on a container of containers.
 *
 * For instance, this can be used to treat a `std::vector<std::vector<T>>` as
 * a single list of `T`.
 */
template <
    typename OuterIterator,
    typename InnerIterator,
    typename Dereference = FlattenDereference<OuterIterator>>
class FlattenIterator {
 public:
  using OuterReference =
      typename std::iterator_traits<OuterIterator>::reference;

 public:
  static_assert(std::is_same_v<
                decltype(Dereference::begin(std::declval<OuterReference>())),
                InnerIterator>);
  static_assert(std::is_same_v<
                decltype(Dereference::end(std::declval<OuterReference>())),
                InnerIterator>);

 public:
  // C++ iterator concept member types
  using iterator_category = std::forward_iterator_tag;
  using value_type = typename std::iterator_traits<InnerIterator>::value_type;
  using difference_type =
      typename std::iterator_traits<OuterIterator>::difference_type;
  using pointer = typename std::iterator_traits<InnerIterator>::pointer;
  using reference = typename std::iterator_traits<InnerIterator>::reference;

 public:
  explicit FlattenIterator(OuterIterator begin, OuterIterator end)
      : outer_(Range<OuterIterator>{std::move(begin), std::move(end)}),
        inner_(std::nullopt) {
    if (outer_.begin == outer_.end) {
      return;
    }
    inner_ = Range<InnerIterator>{
        Dereference::begin(*outer_.begin), Dereference::end(*outer_.begin)};
    advance_empty();
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(FlattenIterator)

  FlattenIterator& operator++() {
    ++inner_->begin;
    advance_empty();
    return *this;
  }

  FlattenIterator operator++(int) {
    FlattenIterator result = *this;
    ++(*this);
    return result;
  }

  bool operator==(const FlattenIterator& other) const {
    return outer_.begin == other.outer_.begin &&
        ((!inner_.has_value() && !other.inner_.has_value()) ||
         (inner_.has_value() && other.inner_.has_value() &&
          inner_->begin == other.inner_->begin));
  }

  bool operator!=(const FlattenIterator& other) const {
    return !(*this == other);
  }

  reference operator*() {
    return *inner_->begin;
  }

 private:
  /* Advance the iterator until we find an element. */
  void advance_empty() {
    while (inner_->begin == inner_->end) {
      ++outer_.begin;
      if (outer_.begin == outer_.end) {
        inner_ = std::nullopt;
        return;
      } else {
        inner_ = Range<InnerIterator>{
            Dereference::begin(*outer_.begin), Dereference::end(*outer_.begin)};
      }
    }
  }

 private:
  template <typename T>
  struct Range {
    T begin;
    T end;
  };

 private:
  Range<OuterIterator> outer_;
  std::optional<Range<InnerIterator>> inner_;
};

} // namespace marianatrench
