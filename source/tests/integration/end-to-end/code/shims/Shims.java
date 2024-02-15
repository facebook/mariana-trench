/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;
import android.os.Bundle;
import androidx.fragment.app.FragmentActivity;

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

class FragmentOneActivity extends FragmentActivity {
  private Bundle mArguments;

  FragmentOneActivity() {}

  @Override
  protected void onCreate(Bundle savedInstanceState) {}

  @Override
  public void onStart() {
    Origin.sink(getArguments());
  }

  public void onTest(Object o) {
    Origin.sink(o);
  }

  void setArguments(Bundle b) {
    this.mArguments = b;
  }

  Bundle getArguments() {
    return this.mArguments;
  }
}

class FragmentTwoActivity extends FragmentActivity {
  FragmentTwoActivity() {}
}

class FragmentThreeActivity extends FragmentActivity {
  FragmentThreeActivity() {}
}

class FragmentTest {
  public void add(Class<? extends FragmentOneActivity> fragmentClass, Object o) {
    // Expect artificial call: <fragmentClass instance>.activity_lifecycle_wrapper()
  }

  public void add(Activity activity, Object o) {
    // Expect artificial call: activity.activity_lifecycle_wrapper()
  }
}

class ShimTarget {
  ShimTarget() {}

  void multipleArguments(int i, String s, Object o) {
    Origin.sink(o);
    Origin.sink(s);
  }

  static void staticMethod(Object o) {
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

  void definedAndInferred(ShimTarget shimTarget, Object o, String s) {
    // Expect artificial call: shimTarget.multipleArguments(<dropped>, s, o)
  }

  void callStatic(Object o) {
    // Expect artificial call: ShimTarget.staticMethod(o)
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
    ft.add(new FragmentOneActivity(), Origin.source());
  }

  static void testFragmentReflection() {
    FragmentTest ft = new FragmentTest();
    ft.add(FragmentOneActivity.class, Origin.source());
  }

  static void testFragmentBaseClass(Activity activity) {
    FragmentTest ft = new FragmentTest();
    // Derived type of Activity is not known here. This should shim to lifecycle
    // methods of all possible derived types, which is just
    // FragmentActivity.activity_lifecycle_wrapper in this case.
    ft.add(activity, Origin.source());
  }

  static void testMultipleFragmentChildClasses(boolean useFragmentOne) {
    Activity fragment = null;
    if (useFragmentOne) {
      fragment = new FragmentOneActivity();
    } else {
      fragment = new FragmentTwoActivity();
    }

    // Expect: FP: Shim to all 3 most derived classes of Activity.
    // even if FragmentThreeActivity is not possible.
    new FragmentTest().add(fragment, Origin.source());
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

  static void testStaticShimTarget() {
    ParameterMapping c = new ParameterMapping();
    c.callStatic(Origin.source());
  }

  static void testSetArguments() {
    Bundle b = new Bundle();
    b.putString("something", (String) Origin.source());

    FragmentOneActivity f = new FragmentOneActivity();
    f.setArguments(b);

    new FragmentTest().add(f, null);
  }

  static void testDefinedAndInferredArgument() {
    ParameterMapping c = new ParameterMapping();
    c.definedAndInferred(new ShimTarget(), Origin.source(), (String) Origin.source());
  }
}
