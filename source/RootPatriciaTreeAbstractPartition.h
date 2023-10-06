/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <initializer_list>
#include <vector>

#include <boost/iterator/transform_iterator.hpp>

#include <sparta/AbstractDomain.h>
#include <sparta/PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

/**
 * A patricia tree map abstract partition from root to the given domain.
 */
template <typename Domain>
class RootPatriciaTreeAbstractPartition final
    : public sparta::AbstractDomain<RootPatriciaTreeAbstractPartition<Domain>> {
 private:
  using Map = sparta::PatriciaTreeMapAbstractPartition<Root, Domain>;

 public:
  // C++ container concept member types
  using key_type = Root;
  using mapped_type = Domain;
  using value_type = std::pair<const Root, Domain>;
  using iterator = typename Map::MapType::iterator;
  using const_iterator = iterator;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const value_type&;
  using const_pointer = const value_type*;

 private:
  explicit RootPatriciaTreeAbstractPartition(Map map) : map_(std::move(map)) {}

 public:
  /* Return the bottom value (i.e, the empty tree). */
  RootPatriciaTreeAbstractPartition() = default;

  RootPatriciaTreeAbstractPartition(
      std::initializer_list<std::pair<Root, Domain>> bindings) {
    for (const auto& [root, value] : bindings) {
      set(root, value);
    }
  }

  RootPatriciaTreeAbstractPartition(
      const std::vector<std::pair<Root, Domain>>& bindings) {
    for (const auto& [root, value] : bindings) {
      set(root, value);
    }
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(
      RootPatriciaTreeAbstractPartition)

  INCLUDE_ABSTRACT_DOMAIN_METHODS(RootPatriciaTreeAbstractPartition, Map, map_)

  /* Return the number of bindings not set to bottom. */
  std::size_t size() const {
    return map_.size();
  }

  iterator begin() const {
    return map_.bindings().begin();
  }

  iterator end() const {
    return map_.bindings().end();
  }

  void difference_with(const RootPatriciaTreeAbstractPartition& other) {
    map_.difference_like_operation(
        other.map_, [](const Domain& left, const Domain& right) {
          if (left.leq(right)) {
            return Domain::bottom();
          }
          return left;
        });
  }

  const Domain& get(Root root) const {
    return map_.get(root);
  }

  void set(Root root, const Domain& value) {
    map_.set(root, value);
  }

  template <typename Operation> // Domain(const Domain&)
  void update(Root root, Operation&& operation) {
    map_.update(root, std::forward<Operation>(operation));
  }

  template <typename Function> // Domain(const Domain&)
  bool transform(Function&& f) {
    return map_.transform(std::forward<Function>(f));
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const RootPatriciaTreeAbstractPartition& value) {
    out << "{";
    for (const auto& [root, elements] : value) {
      out << show(root) << " -> " << elements << ", ";
    }
    return out << "}";
  }

 private:
  Map map_;
};

} // namespace marianatrench
