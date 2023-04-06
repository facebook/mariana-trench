/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

Taint TaintTreeConfiguration::transform_on_widening_collapse(Taint taint) {
  // Add the feature as a may-feature, otherwise we would break the widening
  // invariant: a <= a widen b.
  // Use `FeatureFactory::singleton()` since we have no other way to get the
  // current context.
  taint.add_inferred_features(FeatureMayAlwaysSet::make_may(
      {FeatureFactory::singleton().get_widen_broadening_feature()}));
  return taint;
}

} // namespace marianatrench
