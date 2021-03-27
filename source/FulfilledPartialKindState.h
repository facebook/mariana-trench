/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/FeatureMayAlwaysSet.h>

namespace marianatrench {

/**
 * Represents the state of a fulfilled partial kind (sink).
 * Used by `Transfer` to track the state of partially-fulfilled
 * `MultiSourceMultiSink` rules.
 */
class FulfilledPartialKindState final {
 public:
  explicit FulfilledPartialKindState(const FeatureMayAlwaysSet& features)
      : features_(features) {}
  FulfilledPartialKindState(const FulfilledPartialKindState&) = delete;
  FulfilledPartialKindState(FulfilledPartialKindState&&) = default;
  FulfilledPartialKindState& operator=(const FulfilledPartialKindState&) =
      delete;
  FulfilledPartialKindState& operator=(FulfilledPartialKindState&&) = delete;

  const FeatureMayAlwaysSet& features() const {
    return features_;
  }

 private:
  FeatureMayAlwaysSet features_;
};

} // namespace marianatrench
