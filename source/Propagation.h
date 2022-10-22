/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <json/json.h>

#include <AbstractDomain.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Feature.h>
#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/PathTree.h>

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
      : input_paths_(PathTreeDomain::bottom()),
        inferred_features_(FeatureMayAlwaysSet::bottom()),
        user_features_(FeatureSet::bottom()) {}

  explicit Propagation(
      PathTreeDomain input_paths,
      FeatureMayAlwaysSet inferred_features,
      FeatureSet user_features)
      : input_paths_(std::move(input_paths)),
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
    return input_paths_.is_bottom();
  }

  bool is_top() const override {
    return false;
  }

  void set_to_bottom() override {
    input_paths_.set_to_bottom();
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

  const PathTreeDomain& input_paths() const {
    return input_paths_;
  }

  const FeatureMayAlwaysSet& inferred_features() const {
    return inferred_features_;
  }

  const FeatureSet& user_features() const {
    return user_features_;
  }

  FeatureMayAlwaysSet features() const;

  void truncate(std::size_t size) {
    input_paths_.collapse_deeper_than(size);
  }

  void limit_input_path_leaves(std::size_t max_leaves) {
    input_paths_.limit_leaves(max_leaves);
  }

  // Note that `from_json` takes in a single propagation object per our
  // dsl while `to_json` will return a list of json propagation objects (one for
  // each input path).
  static Propagation from_json(const Json::Value& value, Context& context);
  Json::Value to_json(Root input_root) const;

 private:
  friend std::ostream& operator<<(
      std::ostream& out,
      const Propagation& propagation);

  PathTreeDomain input_paths_;
  FeatureMayAlwaysSet inferred_features_;
  FeatureSet user_features_;
};

} // namespace marianatrench
