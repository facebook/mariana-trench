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

#include <sparta/AbstractDomain.h>
#include <sparta/HashedSetAbstractDomain.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/AccessPathFactory.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/CallClassIntervalContext.h>
#include <mariana-trench/CallInfo.h>
#include <mariana-trench/CallKind.h>
#include <mariana-trench/CanonicalName.h>
#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/ExportOriginsMode.h>
#include <mariana-trench/ExtraTraceSet.h>
#include <mariana-trench/Feature.h>
#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/OriginSet.h>
#include <mariana-trench/PathTreeDomain.h>
#include <mariana-trench/PatriciaTreeSetAbstractDomain.h>
#include <mariana-trench/Position.h>
#include <mariana-trench/TransformKind.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

using RootSetAbstractDomain = PatriciaTreeSetAbstractDomain<
    Root,
    /* bottom_is_empty */ true,
    /* with_top */ false>;
using CanonicalNameSetAbstractDomain =
    sparta::HashedSetAbstractDomain<CanonicalName>;

/**
 * Represents a frame of a trace, i.e a single hop between methods.
 *
 * The `kind` is the label of the taint, e.g "UserInput".
 *
 * The `callee_port` is the port to the next method in the trace, or
 * `Root::Kind::Leaf` for a leaf frame.
 *
 * `callee` is the next method in the trace. This is `nullptr` for a leaf frame.
 *
 * `call_position` is the position of the call to the `callee`. This is
 * `nullptr` for a leaf frame. This can be non-null for leaf frames inside
 * issues, to describe the position of a parameter source or return sink.
 *
 * `distance` is the shortest length of the trace, i.e from this frame to the
 * closest leaf. This is `0` for a leaf frame.
 *
 * `origins` is the set of methods that originated the taint. This is the
 * union of all methods at the end of the trace, i.e the leaves.
 *
 * `features` is a set of tags used to give extra information about the trace.
 * For instance, "via-numerical-operator" could be used to express that the
 * trace goes through a numerical operator. Internally, this is represented by:
 *   inferred_features:
 *     Features propagated into this frame, usually from its callee.
 *   user_features:
 *     User-defined features from a JSON.
 *
 * `via_type_of_ports` is a set of ports for each of which we would like to
 * materialize a 'via-type-of' feature with the type of the port seen at a
 * callsite and include it in the inferred features of the taint at that
 * callsite

 * `via_value_of_ports` is a set of ports for each of which we would like to
 * materialize a 'via-value-of' feature with the value of the port seen at a
 * callsite and include it in the inferred features of the taint at that
 * callsite
 *
 * `canonical_names` is used for cross-repo taint exchange (crtex) which
 * requires that callee names at the leaves conform to a naming format. This
 * format is defined using placeholders. See `CanonicalName`.
 *
 * `output_paths` is used to infer propagations with the `local_result` and
 * `receiver` kinds in the backward analysis.
 */
class Frame final : public sparta::AbstractDomain<Frame> {
 public:
  /* Create the bottom frame. */
  explicit Frame()
      : kind_(nullptr),
        callee_port_(nullptr),
        callee_(nullptr),
        call_position_(nullptr),
        class_interval_context_(CallClassIntervalContext()),
        distance_(0),
        call_kind_(CallKind::declaration()) {}

  explicit Frame(
      const Kind* kind,
      const AccessPath* callee_port,
      const Method* MT_NULLABLE callee,
      const Position* MT_NULLABLE call_position,
      CallClassIntervalContext class_interval_context,
      int distance,
      OriginSet origins,
      FeatureMayAlwaysSet inferred_features,
      FeatureSet user_features,
      RootSetAbstractDomain via_type_of_ports,
      RootSetAbstractDomain via_value_of_ports,
      CanonicalNameSetAbstractDomain canonical_names,
      CallKind call_kind,
      PathTreeDomain output_paths,
      ExtraTraceSet extra_traces)
      : kind_(kind),
        callee_port_(callee_port),
        callee_(callee),
        call_position_(call_position),
        class_interval_context_(std::move(class_interval_context)),
        distance_(distance),
        origins_(std::move(origins)),
        inferred_features_(std::move(inferred_features)),
        user_features_(std::move(user_features)),
        via_type_of_ports_(std::move(via_type_of_ports)),
        via_value_of_ports_(std::move(via_value_of_ports)),
        canonical_names_(std::move(canonical_names)),
        call_kind_(std::move(call_kind)),
        output_paths_(std::move(output_paths)),
        extra_traces_(std::move(extra_traces)) {
    mt_assert(kind_ != nullptr);
    mt_assert(callee_port_ != nullptr);
    mt_assert(distance_ >= 0);

    if (auto* propagation_kind =
            kind_->discard_transforms()->as<PropagationKind>()) {
      mt_assert(!output_paths_.is_bottom());
      mt_assert(call_kind_.is_propagation());
      if (call_kind_.is_propagation_without_trace()) {
        // Retaining previous invariant of callee port == output port
        // for propagations without traces.
        mt_assert(*callee_port_ == AccessPath(propagation_kind->root()));
      }
    }
    if (callee != nullptr && !callee_port_->root().is_anchor()) {
      mt_assert(call_kind.is_callsite());
    }
  }

  explicit Frame(const TaintConfig& config);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Frame)

  /* Return the kind, or `nullptr` for bottom. */
  const Kind* MT_NULLABLE kind() const {
    return kind_;
  }

  /* If this frame represents a propagation, return the PropagationKind.
   * Asserts otherwise. */
  const PropagationKind* propagation_kind() const;

  const AccessPath* callee_port() const {
    mt_assert(callee_port_ != nullptr);
    return callee_port_;
  }

  /* Return the callee, or `nullptr` if this is a leaf frame. */
  const Method* MT_NULLABLE callee() const {
    return callee_;
  }

  /* Return the position of the call, or `nullptr` if this is a leaf frame. */
  const Position* MT_NULLABLE call_position() const {
    return call_position_;
  }

  const CallClassIntervalContext& class_interval_context() const {
    return class_interval_context_;
  }

  int distance() const {
    return distance_;
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

  void add_origins_if_declaration(const Method* method, const AccessPath* port);

  void add_origins_if_declaration(const Field* field);

  const OriginSet& origins() const {
    return origins_;
  }

  const CallKind& call_kind() const {
    return call_kind_;
  }

  CallInfo call_info() const {
    return CallInfo(callee_, call_kind_, callee_port_, call_position_);
  }

  void append_to_propagation_output_paths(Path::Element path_element);

  void update_maximum_collapse_depth(CollapseDepth collapse_depth);

  const PathTreeDomain& output_paths() const {
    return output_paths_;
  }

  void add_inferred_features(const FeatureMayAlwaysSet& features);

  void add_user_features(const FeatureSet& features);

  const FeatureMayAlwaysSet& inferred_features() const {
    return inferred_features_;
  }

  const FeatureSet& user_features() const {
    return user_features_;
  }

  FeatureMayAlwaysSet features() const;

  void add_extra_trace(const Frame& propagation_frame);

  const ExtraTraceSet& extra_traces() const {
    return extra_traces_;
  }

  static Frame bottom() {
    return Frame();
  }

  static Frame top() {
    mt_unreachable(); // Not implemented.
  }

  bool is_bottom() const {
    return kind_ == nullptr;
  }

  bool is_top() const {
    return false;
  }

  bool is_leaf() const {
    return callee_ == nullptr;
  }

  bool is_crtex_producer_declaration() const {
    // If true, this frame corresponds to the crtex leaf frame declared by
    // the user (callee == nullptr). Also, the producer run declarations use
    // the `Anchor` port, while consumer runs use the `Producer` port.
    return kind_ != nullptr && callee_ == nullptr &&
        callee_port_->root().is_anchor();
  }

  void set_to_bottom() {
    kind_ = nullptr;
  }

  void set_to_top() {
    mt_unreachable(); // Not implemented.
  }

  bool leq(const Frame& other) const;

  bool equals(const Frame& other) const;

  void join_with(const Frame& other);

  void widen_with(const Frame& other);

  void meet_with(const Frame& other);

  void narrow_with(const Frame& other);

  /* Return frame with the given kind (and every other field kept the same) */
  Frame with_kind(const Kind* kind) const;

  Frame update_with_propagation_trace(const Frame& propagation_frame) const;

  Frame apply_transform(
      const KindFactory& kind_factory,
      const TransformsFactory& transforms_factory,
      const UsedKinds& used_kinds,
      const TransformList* local_transforms) const;

  std::vector<const Feature*> materialize_via_type_of_ports(
      const Method* callee,
      const FeatureFactory* feature_factory,
      const std::vector<const DexType * MT_NULLABLE>& source_register_types)
      const;

  std::vector<const Feature*> materialize_via_value_of_ports(
      const Method* callee,
      const FeatureFactory* feature_factory,
      const std::vector<std::optional<std::string>>& source_constant_arguments)
      const;

  void set_call_position(const Position* MT_NULLABLE call_position);
  Frame with_call_position_and_origins(
      const Position* MT_NULLABLE call_position,
      OriginSet origins) const;

  Json::Value to_json(ExportOriginsMode export_origins_mode) const;

  friend std::ostream& operator<<(std::ostream& out, const Frame& frame);

 private:
  const Kind* MT_NULLABLE kind_;
  const AccessPath* MT_NULLABLE callee_port_;
  const Method* MT_NULLABLE callee_;
  const Position* MT_NULLABLE call_position_;
  CallClassIntervalContext class_interval_context_;
  int distance_;
  OriginSet origins_;
  FeatureMayAlwaysSet inferred_features_;
  FeatureSet user_features_;
  RootSetAbstractDomain via_type_of_ports_;
  RootSetAbstractDomain via_value_of_ports_;
  CanonicalNameSetAbstractDomain canonical_names_;
  CallKind call_kind_;
  PathTreeDomain output_paths_;
  ExtraTraceSet extra_traces_;
};

} // namespace marianatrench
