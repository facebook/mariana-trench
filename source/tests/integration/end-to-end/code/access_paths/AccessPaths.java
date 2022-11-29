/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import java.util.Random;

class Dictionary {
  Object field;

  // Dictionary like stubs
  void setIndex(String key, Object value) {}

  Object getIndex(String key) {
    return new Object();
  }
}

class Constants {
  public static final String FOO = "foo";
}

public class AccessPaths {
  public static void testSimple() {
    Dictionary d = new Dictionary();
    d.field = new Object();
    d.setIndex("safe_field", new Object());
    d.setIndex("field", Origin.source());

    Origin.sink(d.field); // expect no issue
    Origin.sink(d.getIndex("safe_field")); // expect no issue
    Origin.sink(d.getIndex("field")); // expect issue
  }

  private static String getString() {
    return "string";
  }

  private static Object getDifferentSource() {
    return new Object();
  }

  public static void testStrongUpdate() {
    Dictionary d = new Dictionary();

    d.setIndex(Constants.FOO, Origin.source());
    d.setIndex(Constants.FOO, getDifferentSource());

    Origin.sink(d.getIndex(Constants.FOO)); // FP: expect issue for DifferentSource only
  }

  public static void testWeakUpdate() {
    Dictionary d = new Dictionary();
    Random rand = new Random();

    if (rand.nextInt(2) == 0) {
      d.setIndex(Constants.FOO, Origin.source());
    } else {
      d.setIndex(Constants.FOO, new Object());
    }

    Origin.sink(d.getIndex(Constants.FOO)); // expect issue
  }

  public static void testUnresolvedKey() {
    Dictionary d = new Dictionary();

    d.setIndex(getString(), Origin.source());
    d.setIndex(Constants.FOO, getDifferentSource());

    Origin.sink(d.getIndex(Constants.FOO)); // FP: expect issue for DifferentSource only
    Origin.sink(d.getIndex("unknown")); // expect issues for Source only
    Origin.sink(d.getIndex(getString())); // expect issues for both sources
  }
}
