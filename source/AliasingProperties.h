/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/AbstractDomain.h>

#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/LocalPositionSet.h>

namespace marianatrench {

class AliasingProperties final
    : public sparta::AbstractDomain<AliasingProperties> {
 public:
  /**
   * Default constructor as required by sparta AbstractDomain.
   * Do not use directly.
   * Also, this creates an bottom (not empty) aliasing properties to
   * discourage its use. Prefer the named constructors below.
   *
   * We differentiate between the `empty` and `bottom` aliasing properties
   * because this is stored in a PatriciaTreeMapAbstractPartition which does not
   * explicitly represent bindings of a label to the Bottom element (i.e.
   * everything is bottom by default).
   *
   * `bottom` vs `empty` element is distinguished by the `local_positions_`
   * where using `bottom()` is not desired because `bottom().add(new_position)`
   * gives `bottom()`.
   */
  AliasingProperties()
      : local_positions_(LocalPositionSet::bottom()),
        locally_inferred_features_(FeatureMayAlwaysSet::bottom()) {}

 private:
  AliasingProperties(
      LocalPositionSet local_positions,
      FeatureMayAlwaysSet features)
      : local_positions_(std::move(local_positions)),
        locally_inferred_features_(std::move(features)) {}

 public:
  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(AliasingProperties)

  static AliasingProperties empty() {
    return AliasingProperties(
        /* local_positions */ {}, /* features */ {});
  }

  static AliasingProperties bottom() {
    return AliasingProperties();
  }

  [[noreturn]] static AliasingProperties top() {
    mt_unreachable();
  }

  bool is_bottom() const {
    return local_positions_.is_bottom() &&
        locally_inferred_features_.is_bottom();
  }

  bool is_top() const {
    return local_positions_.is_top() && locally_inferred_features_.is_top();
  }

  bool is_empty() const {
    return local_positions_.empty() && locally_inferred_features_.empty();
  }

  void set_to_empty() {
    local_positions_ = {};
    locally_inferred_features_ = {};
  }

  [[noreturn]] void set_to_bottom() {
    mt_unreachable();
  }

  [[noreturn]] void set_to_top() {
    mt_unreachable();
  }

  bool leq(const AliasingProperties& other) const;

  bool equals(const AliasingProperties& other) const;

  void join_with(const AliasingProperties& other);

  void widen_with(const AliasingProperties& other);

  void meet_with(const AliasingProperties& other);

  void narrow_with(const AliasingProperties& other);

  void difference_with(const AliasingProperties& other);

  const LocalPositionSet& local_positions() const {
    return local_positions_;
  }

  const FeatureMayAlwaysSet& locally_inferred_features() const {
    return locally_inferred_features_;
  }

  void add_local_position(const Position* position);

  void add_locally_inferred_features(const FeatureMayAlwaysSet& features);

  friend std::ostream& operator<<(
      std::ostream& out,
      const AliasingProperties& properties);

 private:
  LocalPositionSet local_positions_;
  FeatureMayAlwaysSet locally_inferred_features_;
};

} // namespace marianatrench
