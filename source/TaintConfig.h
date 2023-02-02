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

#include <mariana-trench/AbstractTreeDomain.h>
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
#include <mariana-trench/SingletonAbstractDomain.h>

namespace marianatrench {

using PathTreeDomain = AbstractTreeDomain<SingletonAbstractDomain>;

/**
 * Class used to contain details for building a `Taint` object.
 * Currently looks very similar to `Frame` because most of the fields in
 * `Taint` are stored in `Frame`. However, it also contains fields that are
 * stored outside of `Frame` (but within `Taint`). Multiple `TaintConfigs` can
 * be used to create a `Taint` object. The `Taint` object is responsible for
 * merging fields accordingly.
 */
class TaintConfig final {
 public:
  explicit TaintConfig(
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
      PathTreeDomain input_paths,
      PathTreeDomain output_paths,
      LocalPositionSet local_positions,
      CallInfo call_info)
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
        input_paths_(std::move(input_paths)),
        output_paths_(std::move(output_paths)),
        local_positions_(std::move(local_positions)),
        call_info_(call_info) {
    mt_assert(kind_ != nullptr);
    mt_assert(distance_ >= 0);
    mt_assert(!(callee && field_callee));
    mt_assert(!local_positions.is_bottom());
    mt_assert(!is_artificial_source() || !is_result_or_receiver_sink());
    if (!is_artificial_source()) {
      mt_assert(input_paths_.is_bottom());
    } else {
      mt_assert(callee_port_.path().empty());
    }

    if (!is_result_or_receiver_sink()) {
      mt_assert(output_paths_.is_bottom());
    } else {
      if (kind_ == Kinds::local_result()) {
        mt_assert(callee_port == AccessPath(Root(Root::Kind::Return)));
      } else {
        mt_assert(callee_port == AccessPath(Root(Root::Kind::Argument, 0)));
      }
    }
  }

  TaintConfig(const TaintConfig&) = default;
  TaintConfig(TaintConfig&&) = default;
  TaintConfig& operator=(const TaintConfig&) = default;
  TaintConfig& operator=(TaintConfig&&) = default;

  friend bool operator==(const TaintConfig& self, const TaintConfig& other) {
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
        self.input_paths_ == other.input_paths_ &&
        self.output_paths_ == other.output_paths_ &&
        self.local_positions_ == other.local_positions_ &&
        self.call_info_ == other.call_info_;
  }

  friend bool operator!=(const TaintConfig& self, const TaintConfig& other) {
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

  const PathTreeDomain& input_paths() const {
    return input_paths_;
  }

  const PathTreeDomain& output_paths() const {
    return output_paths_;
  }

  const LocalPositionSet& local_positions() const {
    return local_positions_;
  }

  CallInfo call_info() const {
    return call_info_;
  }

  bool is_artificial_source() const {
    return kind_ == Kinds::artificial_source();
  }

  bool is_result_or_receiver_sink() const {
    return kind_ == Kinds::local_result() || kind_ == Kinds::receiver();
  }

  bool is_leaf() const {
    return callee_ == nullptr;
  }

  static TaintConfig from_json(const Json::Value& value, Context& context);

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
  // TODO (T135528735) this should be removed in favor of output_paths_ once
  // backward analysis is added. These are used only for artificial sources
  // (should be bottom in all other cases). They are used for propagation/sink
  // inference in forward analysis.
  PathTreeDomain input_paths_;
  // These are used only for result and receiver sinks (should be bottom in all
  // other cases). They are used for propagation/sink inference in backward
  // analysis.
  PathTreeDomain output_paths_;

  /**
   * Properties that are unique to `CalleePortFrames` within `Taint`. If a
   * `Taint` object is constructed from multiple configs with different such
   * values, they will be joined at the callee_port level, i.e. `Frame`s with
   * the same (kind, callee, call_position, callee_port) will share these values
   * even if only some `TaintConfig`s contain it.
   */
  LocalPositionSet local_positions_;
  CallInfo call_info_;
};

} // namespace marianatrench
