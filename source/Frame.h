/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <ostream>

#include <json/json.h>

#include <AbstractDomain.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Feature.h>
#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/Kinds.h>
#include <mariana-trench/LocalPositionSet.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/MethodSet.h>
#include <mariana-trench/Position.h>

namespace marianatrench {

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
 * trace goes through a numerical operator.
 *
 * `local_positions` is the set of positions that the taint flowed through
 * within the current method.
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
        distance_(0) {}

  explicit Frame(
      const Kind* kind,
      AccessPath callee_port,
      const Method* MT_NULLABLE callee,
      const Position* MT_NULLABLE call_position,
      int distance,
      MethodSet origins,
      FeatureMayAlwaysSet inferred_features,
      FeatureSet user_features,
      LocalPositionSet local_positions)
      : kind_(kind),
        callee_port_(std::move(callee_port)),
        callee_(callee),
        call_position_(call_position),
        distance_(distance),
        origins_(std::move(origins)),
        inferred_features_(std::move(inferred_features)),
        user_features_(std::move(user_features)),
        local_positions_(std::move(local_positions)) {
    mt_assert(kind_ != nullptr);
    mt_assert(distance_ >= 0);
    mt_assert(!local_positions_.is_bottom());
  }

  static Frame leaf(
      const Kind* kind,
      FeatureMayAlwaysSet inferred_features = FeatureMayAlwaysSet::bottom(),
      FeatureSet user_features = FeatureSet::bottom(),
      MethodSet origins = {}) {
    return Frame(
        kind,
        /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
        /* callee */ nullptr,
        /* call_position */ nullptr,
        /* distance */ 0,
        origins,
        inferred_features,
        user_features,
        /* local_positions */ {});
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

  /* Return the position of the call, or `nullptr` if this is a leaf frame. */
  const Position* MT_NULLABLE call_position() const {
    return call_position_;
  }

  int distance() const {
    return distance_;
  }

  void set_origins(const MethodSet& origins);

  const MethodSet& origins() const {
    return origins_;
  }

  void add_inferred_features(const FeatureMayAlwaysSet& features);

  const FeatureMayAlwaysSet& inferred_features() const {
    return inferred_features_;
  }

  const FeatureSet& user_features() const {
    return user_features_;
  }

  FeatureMayAlwaysSet features() const;

  void add_local_position(const Position* position);

  void set_local_positions(LocalPositionSet positions);

  const LocalPositionSet& local_positions() const {
    return local_positions_;
  }

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

  /**
   * Return an artificial source for the given parameter.
   *
   * An artificial source is a source used to track the flow of a parameter,
   * to infer sinks and propagations. Instead of relying on a backward analysis,
   * we introduce these artificial sources in the forward analysis. This saves
   * the maintenance cost of having a forward and backward transfer function.
   */
  static Frame artificial_source(ParameterPosition parameter_position);
  static Frame artificial_source(AccessPath access_path);

  bool is_artificial_source() const {
    return kind_ == Kinds::artificial_source();
  }

  /* Append a field to the callee port. Only safe for artificial sources. */
  void callee_port_append(Path::Element path_element);

  /* Return frame with the given kind (and every other field kept the same) */
  Frame with_kind(const Kind* kind) const;

  static Frame from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  // Describe how to join frames together in `FrameSet`.
  struct GroupEqual {
    bool operator()(const Frame& left, const Frame& right) const;
  };

  // Describe how to join frames together in `FrameSet`.
  struct GroupHash {
    std::size_t operator()(const Frame& frame) const;
  };

  friend std::ostream& operator<<(std::ostream& out, const Frame& frame);

 private:
  const Kind* MT_NULLABLE kind_;
  AccessPath callee_port_;
  const Method* MT_NULLABLE callee_;
  const Position* MT_NULLABLE call_position_;
  int distance_;
  MethodSet origins_;
  FeatureMayAlwaysSet inferred_features_;
  FeatureSet user_features_;
  LocalPositionSet local_positions_;
};

} // namespace marianatrench