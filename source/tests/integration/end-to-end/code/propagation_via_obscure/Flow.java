/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
