/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Recursion {
  public static Object selfRecursive(Object x) {
    if (x == null) {
      return null;
    }
    return selfRecursive(x);
  }

  public static Object mutualA(Object x) {
    if (x == null) {
      return null;
    }
    return mutualB(x);
  }

  public static Object mutualB(Object x) {
    return mutualA(x);
  }
}
