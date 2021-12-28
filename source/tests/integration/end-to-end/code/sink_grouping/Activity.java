/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Activity {

  public void onCreate() {
    String source = (String) Origin.source();
    Origin.sink(source);
    sink(source);
  }

  public void sink(String value) {}
}
