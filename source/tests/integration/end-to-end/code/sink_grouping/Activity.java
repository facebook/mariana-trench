// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

public class Activity {

  public void onCreate() {
    String source = (String) Origin.source();
    Origin.sink(source);
    sink(source);
  }

  public void sink(String value) {}
}
