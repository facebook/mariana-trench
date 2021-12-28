/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_map>

#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/MethodContext.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/TriggeredPartialKind.h>

namespace marianatrench {

/**
 * Represents the state of a fulfilled partial kind (sink).
 * Used by `Transfer` to track the state of partially-fulfilled
 * `MultiSourceMultiSink` rules.
 */
class FulfilledPartialKindState final {
 private:
  using Value = FeatureMayAlwaysSet;
  using RuleMap = std::unordered_map<const MultiSourceMultiSinkRule*, Value>;

 public:
  FulfilledPartialKindState() = default;
  FulfilledPartialKindState(const FulfilledPartialKindState&) = delete;
  FulfilledPartialKindState(FulfilledPartialKindState&&) = delete;
  FulfilledPartialKindState& operator=(const FulfilledPartialKindState&) =
      delete;
  FulfilledPartialKindState& operator=(FulfilledPartialKindState&&) = delete;

  /**
   * Called when sink `kind` is fulfilled under `rule`, i.e. has a matching
   * source flow into the sink as defined by the rule.
   *
   * `features` is the combined set of features from the source and sink flow
   * of the fulfilled rule.
   * `context` MethodContext of the method where the kind was fulfilled.
   * `sink` FrameSet of the sink portion of the fulfilled flow.
   *
   * Returns `std::nullopt` if rule is only half fulfilled, or a FrameSet
   * representing the sink flow of the issue if both parts of the rule is
   * fulfilled.
   */
  std::optional<FrameSet> fulfill_kind(
      const PartialKind* kind,
      const MultiSourceMultiSinkRule* rule,
      const FeatureMayAlwaysSet& features,
      MethodContext* context,
      const FrameSet& sink);

  /**
   * Given an `unfufilled_kind`, check if its counterpart flow has been
   * fulfilled under the given rule. Returns the `PartialKind` of the fulfilled
   * counterpart, or nullptr if the counterpart was not fulfilled.
   */
  const PartialKind* MT_NULLABLE get_fulfilled_counterpart(
      const PartialKind* unfulfilled_kind,
      const MultiSourceMultiSinkRule* rule) const;

  /**
   * Returns features of the flow where `kind` was fulfilled under `rule`.
   */
  FeatureMayAlwaysSet get_features(
      const PartialKind* kind,
      const MultiSourceMultiSinkRule* rule) const;

  /**
   * Given an `unfulfilled_kind`, create its `TriggeredPartialKind`s from any
   * fulfilled counterparts. There can be more than one resulting triggered
   * kind because it may have fulfilled counterparts in more than one rule.
   */
  std::vector<const Kind*> make_triggered_counterparts(
      MethodContext* context,
      const PartialKind* unfulfilled_kind) const;

 private:
  void add_fulfilled_kind(
      const PartialKind* kind,
      const MultiSourceMultiSinkRule* rule,
      const FeatureMayAlwaysSet& features);
  void erase(const PartialKind* kind, const MultiSourceMultiSinkRule* rule);

  std::unordered_map<const PartialKind*, RuleMap> map_;
};

} // namespace marianatrench
