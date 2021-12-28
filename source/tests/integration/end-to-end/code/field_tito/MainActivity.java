/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
