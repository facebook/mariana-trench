/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

/** FP case for builder pattern when "alias-memory-location-on-invoke" is present */
class SanitizeTransformsWithBuilder {

  public static final class Builder {
    int number;

    // Source "builder"
    public Builder() {
      number = 0;
    }

    // Sanitizes sink "build"
    // Has "alias-memory-location-on-invoke" in its model
    public Builder number(int n) {
      number = n;
      return this;
    }

    // Sink "build"
    public SanitizeTransformsWithBuilder build() {
      return new SanitizeTransformsWithBuilder();
    }
  }

  // FP: MT should not report an issue here, but it does
  static SanitizeTransformsWithBuilder testBuilderNoIssue() {
    return new Builder().number(42).build();
  }
}
