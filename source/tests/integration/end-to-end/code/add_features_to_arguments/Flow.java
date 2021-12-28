/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {

  void applyFeature(Object attach_features, Object no_attached_features) {}

  void hop1(Object argument1, Object argument2) {
    // Switching the argument port, to make sure we infer the features on the argument corresponding
    // to the caller (Argument(2) here), not the callee (Argument(1))
    applyFeature(argument2, argument1);
  }

  void hop2(Object argument) {
    Object thing = new Object();
    applyFeature(thing, argument);
  }

  void flow() {
    Object expect_feature = Origin.source();
    Object source1 = Origin.source();
    Object source2 = Origin.source();
    hop1(source1, expect_feature);
    hop2(source2);

    Origin.sink(expect_feature);
    Origin.sink(source1);
    Origin.sink(source2);
  }
}
