/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

interface Runnable {
  void run();
}

interface Executor {
  void execute(Runnable runnable);
}

public class RunnableFlows {

  static void noFlow() {
    Object data = Origin.source();
    Executor executor = (Executor) new Object();
    executor.execute(
        new Runnable() {
          @Override
          public void run() {
            // No data into sink.
          }
        });
  }

  static void flow() {
    final Object data = Origin.source();
    Executor executor = (Executor) new Object();
    executor.execute(
        new Runnable() {
          @Override
          public void run() {
            Origin.sink(data);
          }
        });
  }
}
