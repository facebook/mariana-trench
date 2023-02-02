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
#include <mariana-trench/Kind.h>
#include <mariana-trench/Kinds.h>
#include <mariana-trench/LocalPositionSet.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/MethodSet.h>
#include <mariana-trench/Position.h>

namespace marianatrench {

using RootSetAbstractDomain = sparta::HashedSetAbstractDomain<Root>;
using CanonicalNameSetAbstractDomain =
    sparta::HashedSetAbstractDomain<CanonicalName>;

using KindEncoding = unsigned int;

enum class CallInfo : KindEncoding {
  // A declaration of taint from a model - should not be included in the final
  // trace.
  Declaration = 1,
  // The origin frame for taint that indicates a leaf.
  Origin = 2,
  // A call site where taint is propagated from some origin.
  CallSite = 3,
};

const std::string show_call_info(CallInfo call_info);
CallInfo propagate_call_info(CallInfo call_info);

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
 * `field_callee` is set only if this frame is a frame within a field model or
 * if it represents taint in a method model that came from a field access. Both
 * callee and field_callee should never be set in one frame.
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
 *   locally_inferred_features:
 *     Features inferred within this frame (not propagated from a callee).
 *     These features are only ever added after frame creation.
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
 * For artificial sources, the callee port is used as the origin of the source.
 */
class Frame final : public sparta::AbstractDomain<Frame> {
 public:
  /* Create the bottom frame. */
  explicit Frame()
      : kind_(nullptr),
        callee_port_(Root(Root::Kind::Leaf)),
        callee_(nullptr),
        call_position_(nullptr),
        distance_(0),
        call_info_(CallInfo::Declaration) {}

  explicit Frame(
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
        call_info_(call_info) {
    mt_assert(kind_ != nullptr);
    mt_assert(distance_ >= 0);
    mt_assert(!(callee && field_callee));
    if (kind_ == Kinds::local_result()) {
      mt_assert(callee_port == AccessPath(Root(Root::Kind::Return)));
    } else if (kind_ == Kinds::receiver()) {
      mt_assert(callee_port == AccessPath(Root(Root::Kind::Argument, 0)));
    } else if (kind == Kinds::artificial_source()) {
      mt_assert(
          callee_port.root().kind() == Root::Kind::Argument &&
          callee_port.path().empty());
    }
    if (callee != nullptr && !callee_port_.root().is_anchor()) {
      mt_assert(call_info == CallInfo::CallSite);
    }
  }

  Frame(const Frame&) = default;
  Frame(Frame&&) = default;
  Frame& operator=(const Frame&) = default;
  Frame& operator=(Frame&&) = default;

  /* Return the kind, or `nullptr` for bottom. */
  const Kind* MT_NULLABLE kind() const {
    return kind_;
  }

  const AccessPath& callee_port() const {
    return callee_port_;
  }

  /* Return the callee, or `nullptr` if this is a leaf frame. */
  const Method* MT_NULLABLE callee() const {
    return callee_;
  }

  /* Return the field_callee, or `nullptr` if this frame has a method callee or
   * is a leaf. */
  const Field* MT_NULLABLE field_callee() const {
    return field_callee_;
  }

  /* Return the position of the call, or `nullptr` if this is a leaf frame. */
  const Position* MT_NULLABLE call_position() const {
    return call_position_;
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

  void set_origins(const MethodSet& origins);
  void set_field_origins(const FieldSet& field_origins);
  void set_field_callee(const Field* field);

  const MethodSet& origins() const {
    return origins_;
  }

  const FieldSet& field_origins() const {
    return field_origins_;
  }

  CallInfo call_info() const {
    return call_info_;
  }

  /**
   * Despite its name, this adds to locally_inferred_features. Non-local
   * inferred features are used only for frame propagation.
   */
  void add_inferred_features(const FeatureMayAlwaysSet& features);

  const FeatureMayAlwaysSet& inferred_features() const {
    return inferred_features_;
  }

  const FeatureMayAlwaysSet& locally_inferred_features() const {
    return locally_inferred_features_;
  }

  const FeatureSet& user_features() const {
    return user_features_;
  }

  FeatureMayAlwaysSet features() const;

  static Frame bottom() {
    return Frame();
  }

  static Frame top() {
    mt_unreachable(); // Not implemented.
  }

  bool is_bottom() const override {
    return kind_ == nullptr;
  }

  bool is_top() const override {
    return false;
  }

  bool is_leaf() const {
    return callee_ == nullptr;
  }

  bool is_crtex_producer_declaration() const {
    // If true, this frame corresponds to the crtex leaf frame declared by
    // the user (callee == nullptr). Also, the producer run declarations use the
    // `Anchor` port, while consumer runs use the `Producer` port.
    return callee_ == nullptr && callee_port_.root().is_anchor();
  }

  void set_to_bottom() override {
    kind_ = nullptr;
  }

  void set_to_top() override {
    mt_unreachable(); // Not implemented.
  }

  bool leq(const Frame& other) const override;

  bool equals(const Frame& other) const override;

  void join_with(const Frame& other) override;

  void widen_with(const Frame& other) override;

  void meet_with(const Frame& other) override;

  void narrow_with(const Frame& other) override;

  bool is_artificial_source() const {
    return kind_ == Kinds::artificial_source();
  }

  bool is_result_or_receiver_sink() const {
    return kind_ == Kinds::local_result() || kind_ == Kinds::receiver();
  }

  /* Return frame with the given kind (and every other field kept the same) */
  Frame with_kind(const Kind* kind) const;

  Json::Value to_json(const LocalPositionSet& local_positions) const;

  // Describe how to join frames together in `CalleePortFrames`.
  struct GroupEqual {
    bool operator()(const Frame& left, const Frame& right) const;
  };

  // Describe how to join frames together in `CalleePortFrames`.
  struct GroupHash {
    std::size_t operator()(const Frame& frame) const;
  };

  friend std::ostream& operator<<(std::ostream& out, const Frame& frame);

 private:
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
  CallInfo call_info_;
};

} // namespace marianatrench
