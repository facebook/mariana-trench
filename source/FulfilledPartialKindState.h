/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>

namespace marianatrench {

/**
 * Represents the state of a fulfilled partial kind (sink).
 * Used by `Transfer` to track the state of partially-fulfilled
 * `MultiSourceMultiSink` rules. The actual partial kind this applies to
 * is not represented in this class.
 */
class FulfilledPartialKindState final {
 public:
  explicit FulfilledPartialKindState(
      const FeatureMayAlwaysSet& features,
      const MultiSourceMultiSinkRule* rule)
      : features_(features), rule_(rule) {}
  FulfilledPartialKindState(const FulfilledPartialKindState&) = delete;
  FulfilledPartialKindState(FulfilledPartialKindState&&) = default;
  FulfilledPartialKindState& operator=(const FulfilledPartialKindState&) =
      delete;
  FulfilledPartialKindState& operator=(FulfilledPartialKindState&&) = delete;

  /**
   * Features belonging to the flow in which the partial kind (sink) was
   * fulfilled. Includes features from both the source and sink flows.
   */
  const FeatureMayAlwaysSet& features() const {
    return features_;
  }

  /**
   * The rule which caused the partial kind to be fulfilled.
   */
  const MultiSourceMultiSinkRule* rule() const {
    return rule_;
  }

 private:
  FeatureMayAlwaysSet features_;
  const MultiSourceMultiSinkRule* rule_;
};

} // namespace marianatrench
