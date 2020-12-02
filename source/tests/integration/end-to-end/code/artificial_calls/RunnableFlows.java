// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
