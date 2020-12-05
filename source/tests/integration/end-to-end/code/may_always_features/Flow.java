/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  static boolean rand() {
    return true;
  }

  static Object via_one(Object x) {
    return x;
  }

  static Object via_two(Object x) {
    return x;
  }

  static void issue_one() {
    Object x = Origin.source();
    Object y;
    if (rand()) {
      y = via_one(x);
    } else {
      y = via_two(via_one(x));
    }
    // Always via-one, May via-two.
    Origin.sink(y);
  }

  static void sink(Object x) {
    Object y;
    if (rand()) {
      y = via_two(x);
    } else {
      y = via_two(via_one(x));
    }
    // Always via-two, May via-one.
    Origin.sink(y);
  }
}
