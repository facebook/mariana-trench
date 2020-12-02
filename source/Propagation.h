// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <json/json.h>

#include <AbstractDomain.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Feature.h>
#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/FeatureSet.h>

namespace marianatrench {

/**
 * A `Propagation` describes how taint may flow through a method. More
 * specifically, how taint may flow from a parameter to the method's return
 * value or another parameters. A `Propagation` will only propagate the taint
 * if the parameter is tainted.
 *
 * `inferred_features` is a may-always set of features inferred during the
 * analysis. It is bottom for a user-specified propagation.
 *
 * `user_features` is a set of always features specified by the user in model
 * generators.
 */
class Propagation final : public sparta::AbstractDomain<Propagation> {
 public:
  /* Create the bottom (i.e, invalid) propagation. */
  explicit Propagation()
      : input_(Root(Root::Kind::Leaf)),
        inferred_features_(FeatureMayAlwaysSet::bottom()),
        user_features_(FeatureSet::bottom()) {}

  explicit Propagation(
      AccessPath input,
      FeatureMayAlwaysSet inferred_features,
      FeatureSet user_features)
      : input_(std::move(input)),
        inferred_features_(std::move(inferred_features)),
        user_features_(std::move(user_features)) {}

  Propagation(const Propagation&) = default;
  Propagation(Propagation&&) = default;
  Propagation& operator=(const Propagation&) = default;
  Propagation& operator=(Propagation&&) = default;
  ~Propagation() = default;

  static Propagation bottom() {
    return Propagation();
  }

  static Propagation top() {
    mt_unreachable(); // Not implemented.
  }

  bool is_bottom() const override {
    return input_.root().is_leaf();
  }

  bool is_top() const override {
    return false;
  }

  void set_to_bottom() override {
    input_ = AccessPath(Root(Root::Kind::Leaf));
  }

  void set_to_top() override {
    mt_unreachable(); // Not implemented.
  }

  bool leq(const Propagation& other) const override;

  bool equals(const Propagation& other) const override;

  void join_with(const Propagation& other) override;

  void widen_with(const Propagation& other) override;

  void meet_with(const Propagation& other) override;

  void narrow_with(const Propagation& other) override;

  const AccessPath& input() const {
    return input_;
  }

  const FeatureMayAlwaysSet& inferred_features() const {
    return inferred_features_;
  }

  const FeatureSet& user_features() const {
    return user_features_;
  }

  FeatureMayAlwaysSet features() const;

  void truncate(std::size_t size) {
    input_.truncate(size);
  }

  static Propagation from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  // Describe how to join propagations together in `PropagationSet`.
  struct GroupEqual {
    bool operator()(const Propagation& left, const Propagation& right) const;
  };

  // Describe how to join propagations together in `PropagationSet`.
  struct GroupHash {
    std::size_t operator()(const Propagation& propagation) const;
  };

 private:
  friend std::ostream& operator<<(
      std::ostream& out,
      const Propagation& propagation);

  AccessPath input_;
  FeatureMayAlwaysSet inferred_features_;
  FeatureSet user_features_;
};

} // namespace marianatrench
