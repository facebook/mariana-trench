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

#include <sparta/AbstractDomain.h>
#include <sparta/SmallSortedSetAbstractDomain.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/Heuristics.h>
#include <mariana-trench/IncludeMacros.h>
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

  explicit LocalPositionSet(std::initializer_list<const Position*> positions)
      : set_(positions) {}

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(LocalPositionSet)

  INCLUDE_ABSTRACT_DOMAIN_METHODS(LocalPositionSet, Set, set_)

  /* Return true if this is neither top nor bottom. */
  bool is_value() const {
    return set_.is_value();
  }

  bool empty() const {
    return set_.empty();
  }

  const sparta::FlatSet<const Position*>& elements() const {
    return set_.elements();
  }

  void add(const Position* position) {
    set_.add(position);
  }

  static LocalPositionSet from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const LocalPositionSet& positions);

 private:
  Set set_;
};

} // namespace marianatrench
