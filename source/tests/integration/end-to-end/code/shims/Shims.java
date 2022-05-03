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
  Messenger() {
    // No artificial call
  }

  Messenger(Handler handle) {
    // Expect artificial call: handle.handleMessage(<dropped>)
  }

  Messenger(Handler handle, Object o) {
    // Expect artificial call: handler.handleMessage(o)
  }
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
    // Expect artificial call: <fragmentClass instance>.onActivityCreate()
  }

  public void add(Activity activity) {
    // Expect artificial call: activity.onCreate()
  }
}

class ShimTarget {
  ShimTarget() {}

  void multipleArguments(int i, String s, Object o) {
    Origin.sink(o);
  }
}

class ParameterMapping {
  ParameterMapping() {}

  ParameterMapping(Handler handlerOne, Handler handlerTwo, Object objectOne, Object objectTwo) {
    // Expect artificial call: handlerTwo.handlerMessage(objectTwo)
  }

  void inferred(ShimTarget shimTarget, Object o) {
    // Expect artificial call: shimTarget.multipleArguments(<dropped>, <dropped>, o)
  }

  void defined(ShimTarget shimTarget, Object o, String s) {
    // Expect artificial call: shimTarget.multipleArguments(<dropped>, <dropped>, o)
  }
}

public class Shims {
  static Messenger getMessenger(Handler handler) {
    return new Messenger(handler);
  }

  static Messenger testMessengerNoShim() {
    return new Messenger();
  }

  static Messenger testMessageHandlerOneNoIssue() {
    Handler handler = new HandlerOne();
    return new Messenger(handler);
  }

  static Messenger testMessageHandlerOneIssue() {
    Handler handler = new HandlerOne();
    return new Messenger(handler, Origin.source());
  }

  static Messenger testMessageHandlerOneWithHop() {
    return getMessenger(new HandlerOne());
  }

  static Messenger testMessageHandlerTwoWithHop() {
    return getMessenger(new HandlerTwo());
  }

  static void testFragmentInstance() {
    FragmentTest ft = new FragmentTest();
    ft.add(new FragmentActivity());
  }

  static void testFragmentReflection() {
    FragmentTest ft = new FragmentTest();
    // FN: Type of CONST_CLASS is not stored in TypeEnvironment.
    ft.add(FragmentActivity.class);
  }

  static ParameterMapping testParameterMappingIssue() {
    return new ParameterMapping(new Handler(), new HandlerOne(), new Object(), Origin.source());
  }

  static ParameterMapping testParameterMappingNoIssue() {
    return new ParameterMapping(new Handler(), new HandlerOne(), Origin.source(), new Object());
  }

  static void testDirectShimTarget() {
    ShimTarget st = new ShimTarget();
    st.multipleArguments(1, "", Origin.source());
  }

  static void testDropArgumentInferred() {
    ParameterMapping c = new ParameterMapping();
    c.inferred(new ShimTarget(), Origin.source());
  }

  static void testDropArgumentDefined() {
    ParameterMapping c = new ParameterMapping();
    c.defined(new ShimTarget(), Origin.source(), "string");
  }
}
