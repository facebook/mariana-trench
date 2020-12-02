// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

interface RunnableWithArgument {
  void run(Object input);
}

public class RunnableFlows {

  static void noFlow() {
    Object data = Origin.source();
    runRunnable(
        new RunnableWithArgument() {
          @Override
          public void run(Object data) {
            // No data into sink.
          }
        },
        data);
  }

  static void flow() {
    Object data = Origin.source();
    runRunnable(
        new RunnableWithArgument() {
          @Override
          public void run(Object data) {
            Origin.sink(data);
          }
        },
        data);
  }

  static void genericInvocation(RunnableWithArgument runnable) {
    Object data = Origin.source();
    runRunnable(runnable, data);
  }

  static void runRunnable(RunnableWithArgument runnable, Object data) {
    // Run run run...
    runnable.run(data);
  }
}
