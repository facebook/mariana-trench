/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Identity {
  public static Object identity(Object x) {
    return x;
  }

  public static Object twoParams(Object x, Object y) {
    return x;
  }
}
