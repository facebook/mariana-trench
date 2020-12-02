// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

public class MainActivity {

  protected void onCreate() {
    final Event event = new Event();
    event.tainted = (String) Origin.source();
    Origin.sink(event);
  }

  private class Event {
    public String tainted;
  }
}
