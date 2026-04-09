/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Branching {
  public static Object conditional(boolean flag, Object a, Object b) {
    if (flag) {
      return a;
    } else {
      return b;
    }
  }

  public static Object ternary(boolean flag, Object x, Object y) {
    return flag ? x : y;
  }
}
