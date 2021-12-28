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
#include <SmallSortedSetAbstractDomain.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/Heuristics.h>
#include <mariana-trench/Position.h>

namespace marianatrench {

/**
 * Represents the source code positions that taint flows through for a given
 * method.
 */
class LocalPositionSet final : public sparta::AbstractDomain<LocalPositionSet> {
 private:
  using Set = sparta::SmallSortedSetAbstractDomain<
      const Position*,
      Heuristics::kMaxNumberLocalPositions>;

 private:
  explicit LocalPositionSet(Set set) : set_(std::move(set)) {}

 public:
  /* Create the empty position set. */
  LocalPositionSet() = default;

  explicit LocalPositionSet(std::initializer_list<const Position*> positions);

  LocalPositionSet(const LocalPositionSet&) = default;
  LocalPositionSet(LocalPositionSet&&) = default;
  LocalPositionSet& operator=(const LocalPositionSet&) = default;
  LocalPositionSet& operator=(LocalPositionSet&&) = default;

  static LocalPositionSet bottom() {
    return LocalPositionSet(Set::bottom());
  }

  static LocalPositionSet top() {
    return LocalPositionSet(Set::top());
  }

  bool is_bottom() const override {
    return set_.is_bottom();
  }

  bool is_top() const override {
    return set_.is_top();
  }

  /* Return true if this is neither top nor bottom. */
  bool is_value() const {
    return set_.is_value();
  }

  bool empty() const {
    return set_.empty();
  }

  void set_to_bottom() override {
    set_.is_bottom();
  }

  void set_to_top() override {
    set_.is_top();
  }

  const sparta::FlatSet<const Position*>& elements() const {
    return set_.elements();
  }

  void add(const Position* position);

  bool leq(const LocalPositionSet& other) const override;

  bool equals(const LocalPositionSet& other) const override;

  void join_with(const LocalPositionSet& other) override;

  void widen_with(const LocalPositionSet& other) override;

  void meet_with(const LocalPositionSet& other) override;

  void narrow_with(const LocalPositionSet& other) override;

  static LocalPositionSet from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const LocalPositionSet& positions);

 private:
  Set set_;
};

} // namespace marianatrench
