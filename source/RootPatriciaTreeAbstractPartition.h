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

#include <AbstractDomain.h>
#include <PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/Access.h>

namespace marianatrench {

/**
 * A patricia tree map abstract partition from root to the given domain.
 */
template <typename Domain>
class RootPatriciaTreeAbstractPartition final
    : public sparta::AbstractDomain<RootPatriciaTreeAbstractPartition<Domain>> {
 private:
  using Map =
      sparta::PatriciaTreeMapAbstractPartition<Root::IntegerEncoding, Domain>;

  // Since the patricia tree map key needs to be an integer, we need this to
  // transform the key back to a `Root` when iterating on the map.
  struct ExposeBinding {
    const std::pair<Root, Domain>& operator()(
        const std::pair<Root::IntegerEncoding, Domain>& pair) const {
      // This is safe as long as `Root` stores `IntegerEncoding` internally.
      return *reinterpret_cast<const std::pair<Root, Domain>*>(&pair);
    }
  };

 public:
  // C++ container concept member types
  using key_type = Root;
  using mapped_type = Domain;
  using value_type = std::pair<Root, Domain>;
  using iterator =
      boost::transform_iterator<ExposeBinding, typename Map::MapType::iterator>;
  using const_iterator = iterator;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const value_type&;
  using const_pointer = const value_type*;

 private:
  // Safety checks of `boost::transform_iterator`.
  static_assert(std::is_same_v<typename iterator::value_type, value_type>);
  static_assert(
      std::is_same_v<typename iterator::difference_type, difference_type>);
  static_assert(std::is_same_v<typename iterator::reference, const_reference>);
  static_assert(std::is_same_v<typename iterator::pointer, const_pointer>);

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

  RootPatriciaTreeAbstractPartition(const RootPatriciaTreeAbstractPartition&) =
      default;
  RootPatriciaTreeAbstractPartition(RootPatriciaTreeAbstractPartition&&) =
      default;
  RootPatriciaTreeAbstractPartition& operator=(
      const RootPatriciaTreeAbstractPartition&) = default;
  RootPatriciaTreeAbstractPartition& operator=(
      RootPatriciaTreeAbstractPartition&&) = default;

  static RootPatriciaTreeAbstractPartition bottom() {
    return RootPatriciaTreeAbstractPartition(Map::bottom());
  }

  static RootPatriciaTreeAbstractPartition top() {
    return RootPatriciaTreeAbstractPartition(Map::top());
  }

  bool is_bottom() const override {
    return map_.is_bottom();
  }

  bool is_top() const override {
    return map_.is_top();
  }

  void set_to_bottom() override {
    map_.set_to_bottom();
  }

  void set_to_top() override {
    map_.set_to_top();
  }

  /* Return the number of bindings not set to bottom. */
  std::size_t size() const {
    return map_.size();
  }

  iterator begin() const {
    return boost::make_transform_iterator(
        map_.bindings().begin(), ExposeBinding());
  }

  iterator end() const {
    return boost::make_transform_iterator(
        map_.bindings().end(), ExposeBinding());
  }

  bool leq(const RootPatriciaTreeAbstractPartition& other) const override {
    return map_.leq(other.map_);
  }

  bool equals(const RootPatriciaTreeAbstractPartition& other) const override {
    return map_.equals(other.map_);
  }

  void join_with(const RootPatriciaTreeAbstractPartition& other) override {
    map_.join_with(other.map_);
  }

  void widen_with(const RootPatriciaTreeAbstractPartition& other) override {
    map_.widen_with(other.map_);
  }

  void meet_with(const RootPatriciaTreeAbstractPartition& other) override {
    map_.meet_with(other.map_);
  }

  void narrow_with(const RootPatriciaTreeAbstractPartition& other) override {
    map_.narrow_with(other.map_);
  }

  const Domain& get(Root root) const {
    return map_.get(root.encode());
  }

  void set(Root root, const Domain& value) {
    map_.set(root.encode(), value);
  }

  void update(Root root, std::function<Domain(const Domain&)> operation) {
    map_.update(root.encode(), std::move(operation));
  }

  bool map(std::function<Domain(const Domain&)> f) {
    return map_.map(std::move(f));
  }

 private:
  Map map_;
};

} // namespace marianatrench
