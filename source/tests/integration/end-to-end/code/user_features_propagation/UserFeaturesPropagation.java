/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class UserFeaturesPropagation {
  // Expected frame properties:
  //   CallInfo::Declaration
  //   User feature(s): always:source1, always:source2
  public Object source() {
    return new Object();
  }

  // Expected frame properties:
  //   CallInfo::Origin
  //   Locally inferred feature(s): always:source1, always:source2
  // Origin frames should contain user-declared features from the declaration
  // as locally-inferred features.
  public Object sourceHopDistance1() {
    return this.source();
  }

  // Expected frame propeerties:
  //   CallInfo::CallSite
  //   Inferred features: always:source1, always:source2
  // Call-site frames do not contain user features. These are considered
  // (non-locally) inferred/propagated features at this point and must
  // correspond to their respective source kinds.
  public Object sourceHopDistance2() {
    return this.sourceHopDistance1();
  }
}
