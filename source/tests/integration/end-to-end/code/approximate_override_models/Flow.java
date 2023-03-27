/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  public static Dict propagate(Base base, Dict input) {
    return base.propagate(input); // Join A.propagate and B.propagate
  }
}
