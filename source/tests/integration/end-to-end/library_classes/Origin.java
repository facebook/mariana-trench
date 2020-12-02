// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
