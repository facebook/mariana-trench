/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class RunnableActivity {

  public void onCreate() {
    final String tainted = (String) Origin.source();

    Runnable runnable =
        new Runnable() {
          @Override
          public void run() {
            Origin.sink(tainted);
          }
        };

    Thread thread = new Thread(runnable);
    thread.start();
  }
}
