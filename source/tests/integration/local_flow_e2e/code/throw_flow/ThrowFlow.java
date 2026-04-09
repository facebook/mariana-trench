/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class ThrowFlow {
  public static void throwWithMessage(String message) {
    throw new RuntimeException(message);
  }

  public static Object throwOrReturn(boolean flag, Object value) {
    if (flag) {
      throw new IllegalArgumentException("bad");
    }
    return value;
  }
}
