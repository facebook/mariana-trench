/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Origin {
  public static Object source() {
    return new Object();
  }

  public static void sink(Object object) {}

  public static Object sanitize(Object object) {
    return object;
  }
}
