// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

public class Flow {
  interface Appender {
    // Append y to x.
    void append(String x, String y);
  }

  public void issue(Appender appender) {
    String x = new String();
    String y = (String) Origin.source();
    appender.append(x, y);
    Origin.sink(x);
  }
}
