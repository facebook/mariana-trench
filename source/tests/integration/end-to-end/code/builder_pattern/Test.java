/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import java.util.Random;

public class Test {
  // Repro: T90380174: builder pattern missing features.
  public static class Builder {
    String mCertificatePinner;

    public Builder method(String name) {
      // Added so that the method is not trivially inferred as "inline_as_getter"
      String exception = "name == null";
      if (name == null) {
        throw new NullPointerException(exception);
      }
      return this;
    }

    public Builder certificatePinner(String certificatePinner) {
      // Added so that the method is not trivially inferred as "inline_as_getter"
      String exception = "certificatePinner == null";
      if (certificatePinner == null) {
        throw new NullPointerException(exception);
      }
      this.mCertificatePinner = certificatePinner;
      return this;
    }

    public Builder newInstance() {
      return new Builder();
    }

    public Builder maybeNewInstance() {
      Random rand = new Random();
      if (rand.nextInt(2) == 0) {
        return new Builder();
      }

      return this;
    }

    public Test build() {
      return new Test();
    }
  }

  public static Test hasFeature() {
    Test.Builder builder = new Test.Builder();
    // As the return value of the builder methods are ignored,
    // "via-certificate-pinner" feature introduced by mCertificatePinner() is
    // lost  Adding `"inline_as_getter": "Argument(0)"` attribute to the builder
    // pattern model generator resolves this issue.
    builder.method("name").certificatePinner("certificate");

    return builder.build();
  }

  public static Test noFeatureOnNewInstance() {
    Test.Builder builder = new Test.Builder();
    builder.method("name").newInstance().certificatePinner("certificate");

    return builder.build();
  }

  public static Test hasFeatureOnMaybeNewInstance() {
    Test.Builder builder = new Test.Builder();
    builder.method("name").maybeNewInstance().certificatePinner("certificate");

    return builder.build();
  }
}
