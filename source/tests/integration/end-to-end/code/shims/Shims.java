/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;

class Handler {
  void handleMessage(Object o) {}
}

class HandlerOne extends Handler {
  @Override
  void handleMessage(Object o) {
    Origin.sink(o);
  }
}

class HandlerTwo extends Handler {
  @Override
  void handleMessage(Object o) {
    Origin.sink(Origin.source());
  }
}

class Messenger {
  Messenger() {}

  Messenger(Handler handle) {}

  Messenger(Handler handle, Object o) {}
}

class FragmentActivity extends Activity {
  FragmentActivity() {}

  @Override
  protected void onCreate() {
    Origin.sink(Origin.source());
  }
}

class FragmentTest {
  public void add(Class<? extends FragmentActivity> fragmentClass) {
    // Expect artificial call to: <fragmentClass instance>.onActivityCreate()
  }

  public void add(Activity activity) {
    // Expect artificial call to: activity.onCreate()
  }
}

public class Shims {
  static Messenger getMessageHandler(boolean condition) {
    if (condition) {
      return new Messenger(new HandlerOne());
    } else {
      return new Messenger(new HandlerTwo());
    }
  }

  static Messenger messageHandlerOneNoIssue() {
    Handler handler = new HandlerOne();
    return new Messenger(handler);
  }

  static Messenger messageHandlerOneIssue() {
    Handler handler = new HandlerOne();
    return new Messenger(handler, Origin.source());
  }

  static Messenger messageHandlerOneWithHop() {
    return getMessageHandler(true);
  }

  static Messenger messageHandlerTwoWithHop() {
    return getMessageHandler(false);
  }

  static void fragmentInstance() {
    FragmentTest ft = new FragmentTest();
    ft.add(new FragmentActivity());
  }

  static void fragmentReflection() {
    FragmentTest ft = new FragmentTest();
    // FN: Type of CONST_CLASS is not stored in TypeEnvironment.
    ft.add(FragmentActivity.class);
  }
}
