/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class MainActivity {
  Event mEvent;

  protected void call() {
    mEvent = new Event();
    mEvent.tainted = (String) Origin.source();
    return;
  }

  protected void onCreate() {
    this.call();
    String data = this.noCode();
    Origin.sink(data);
  }

  // in models.json this method is mocked as not having code (skip-analysis and TITO)
  // to avoid build failures it is implemented here
  public String noCode() {
    return "";
  }

  private class Event {
    public String tainted;
    public String hardcoded;
  }
}
