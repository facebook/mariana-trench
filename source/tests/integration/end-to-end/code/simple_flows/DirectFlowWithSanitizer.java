// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

public class DirectFlowWithSanitizer {
  public void test() {
    Object x = Origin.source();
    Object y = Origin.sanitize(x);
    Origin.sink(y);
  }
}
