// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
