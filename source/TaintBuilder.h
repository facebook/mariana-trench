/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <ostream>

#include <json/json.h>

#include <AbstractDomain.h>
#include <HashedSetAbstractDomain.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/CanonicalName.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Feature.h>
#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/FieldSet.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/Kinds.h>
#include <mariana-trench/LocalPositionSet.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/MethodSet.h>
#include <mariana-trench/Position.h>

namespace marianatrench {

/**
 * Class used to contain details for building a `Taint` object.
 * Currently looks very similar to `Frame` because most of the fields in
 * `Taint` are stored in `Frame`. However, it also contains fields that are
 * stored outside of `Frame` (but within `Taint`).
 */
class TaintBuilder final {
 public:
  explicit TaintBuilder(
      const Kind* kind,
      AccessPath callee_port,
      const Method* MT_NULLABLE callee,
      const Field* MT_NULLABLE field_callee,
      const Position* MT_NULLABLE call_position,
      int distance,
      MethodSet origins,
      FieldSet field_origins,
      FeatureMayAlwaysSet inferred_features,
      FeatureMayAlwaysSet locally_inferred_features,
      FeatureSet user_features,
      RootSetAbstractDomain via_type_of_ports,
      RootSetAbstractDomain via_value_of_ports,
      CanonicalNameSetAbstractDomain canonical_names,
      LocalPositionSet local_positions)
      : kind_(kind),
        callee_port_(std::move(callee_port)),
        callee_(callee),
        field_callee_(field_callee),
        call_position_(call_position),
        distance_(distance),
        origins_(std::move(origins)),
        field_origins_(std::move(field_origins)),
        inferred_features_(std::move(inferred_features)),
        locally_inferred_features_(std::move(locally_inferred_features)),
        user_features_(std::move(user_features)),
        via_type_of_ports_(std::move(via_type_of_ports)),
        via_value_of_ports_(std::move(via_value_of_ports)),
        canonical_names_(std::move(canonical_names)),
        local_positions_(std::move(local_positions)) {
    mt_assert(kind_ != nullptr);
    mt_assert(distance_ >= 0);
    mt_assert(!(callee && field_callee));
    mt_assert(!local_positions.is_bottom());
  }

  TaintBuilder(const TaintBuilder&) = default;
  TaintBuilder(TaintBuilder&&) = default;
  TaintBuilder& operator=(const TaintBuilder&) = default;
  TaintBuilder& operator=(TaintBuilder&&) = default;

  friend bool operator==(const TaintBuilder& self, const TaintBuilder& other) {
    return self.kind_ == other.kind_ &&
        self.callee_port_ == other.callee_port_ &&
        self.callee_ == other.callee_ &&
        self.field_callee_ == other.field_callee_ &&
        self.call_position_ == other.call_position_ &&
        self.distance_ == other.distance_ && self.origins_ == other.origins_ &&
        self.field_origins_ == other.field_origins_ &&
        self.inferred_features_ == other.inferred_features_ &&
        self.locally_inferred_features_ == other.locally_inferred_features_ &&
        self.user_features_ == other.user_features_ &&
        self.via_type_of_ports_ == other.via_type_of_ports_ &&
        self.via_value_of_ports_ == other.via_value_of_ports_ &&
        self.canonical_names_ == other.canonical_names_ &&
        self.local_positions_ == other.local_positions_;
  }

  friend bool operator!=(const TaintBuilder& self, const TaintBuilder& other) {
    return !(self == other);
  }

  const Kind* MT_NULLABLE kind() const {
    return kind_;
  }

  const AccessPath& callee_port() const {
    return callee_port_;
  }

  const Method* MT_NULLABLE callee() const {
    return callee_;
  }

  const Field* MT_NULLABLE field_callee() const {
    return field_callee_;
  }

  const Position* MT_NULLABLE call_position() const {
    return call_position_;
  }

  int distance() const {
    return distance_;
  }

  const MethodSet& origins() const {
    return origins_;
  }

  const FieldSet& field_origins() const {
    return field_origins_;
  }

  const FeatureMayAlwaysSet& inferred_features() const {
    return inferred_features_;
  }

  const FeatureMayAlwaysSet& locally_inferred_features() const {
    return locally_inferred_features_;
  }

  const FeatureSet& user_features() const {
    return user_features_;
  }

  const RootSetAbstractDomain& via_type_of_ports() const {
    return via_type_of_ports_;
  }

  const RootSetAbstractDomain& via_value_of_ports() const {
    return via_value_of_ports_;
  }

  const CanonicalNameSetAbstractDomain& canonical_names() const {
    return canonical_names_;
  }

  const LocalPositionSet& local_positions() const {
    return local_positions_;
  }

  bool is_artificial_source() const {
    return kind_ == Kinds::artificial_source();
  }

  bool is_leaf() const {
    return callee_ == nullptr;
  }

  void set_origins(const MethodSet& origins) {
    origins_ = origins;
  }

  void set_field_origins(const FieldSet& field_origins) {
    field_origins_ = field_origins;
  }

  void set_field_callee(const Field* MT_NULLABLE field_callee) {
    field_callee_ = field_callee;
  }

  static TaintBuilder from_json(const Json::Value& value, Context& context);

 private:
  /* Properties that are unique to a `Frame` within `Taint`. */
  const Kind* MT_NULLABLE kind_;
  AccessPath callee_port_;
  const Method* MT_NULLABLE callee_;
  const Field* MT_NULLABLE field_callee_;
  const Position* MT_NULLABLE call_position_;
  int distance_;
  MethodSet origins_;
  FieldSet field_origins_;
  FeatureMayAlwaysSet inferred_features_;
  FeatureMayAlwaysSet locally_inferred_features_;
  FeatureSet user_features_;
  RootSetAbstractDomain via_type_of_ports_;
  RootSetAbstractDomain via_value_of_ports_;
  CanonicalNameSetAbstractDomain canonical_names_;

  /**
   * Properties that are unique to `CalleePortFrames` within `Taint`. If a
   * `Taint` object is constructed from multiple builders with different such
   * values, they will be joined at the callee_port level, i.e. `Frame`s with
   * the same (kind, callee, call_position, callee_port) will share these values
   * even if only some `TaintBuilder`s contain it.
   */
  LocalPositionSet local_positions_;
};

} // namespace marianatrench
