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

public class Flow {
  public String tainted;
  public String benign;

  public void noFlow(Executor executor) {
    this.tainted = (String) Origin.source();
    this.benign = "benign";

    executor.execute(
        new Runnable() {
          @Override
          public void run() {
            benignToSink();
          }
        });
  }

  public void flow(Executor executor) {
    this.tainted = (String) Origin.source();
    this.benign = "benign";

    executor.execute(
        new Runnable() {
          @Override
          public void run() {
            taintedToSink();
          }
        });
  }

  public void taintedToSink() {
    Origin.sink(this.tainted);
  }

  public void benignToSink() {
    Origin.sink(this.benign);
  }
}
