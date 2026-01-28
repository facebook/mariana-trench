/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

class ReturnTo {
  ReturnTo() {}

  @Override
  public String toString() {
    return (String) Origin.source();
  }
}

class StringBuilder {
  Object data;

  StringBuilder append(Object o) {
    // Model as obscure with user defined propagation: Argument(1) -> Argument(0)
    return this;
  }

  StringBuilder inlinedAsGetter(Object o) {
    // This will be inlined-as getter.
    return this;
  }

  void inlinedAsSetter(Object o) {
    this.data = o;
  }

  Object get(Object o) {
    return data;
  }

  StringBuilder append(Class klass) {
    // Model as obscure with skip-analysis
    return this;
  }

  Object get(Class klass) {
    return this;
  }
}

public class ReturnToShims {
  static void testReturnToShimmedMethod() {
    ReturnTo returnTo = new ReturnTo();

    StringBuilder sb = new StringBuilder();
    // Expect artificial call: ReturnTo.toString()
    sb.append(returnTo);

    // Expect issue
    Origin.sink(sb);
  }

  static void testReturnToInlinedAsGetter() {
    ReturnTo returnTo = new ReturnTo();

    StringBuilder sb = new StringBuilder();
    // Expect artificial call: ReturnTo.toString()
    sb.inlinedAsGetter(returnTo);

    // Expect issue
    Origin.sink(sb);
  }

  static void testReturnToInlinedAsSetter() {
    ReturnTo returnTo = new ReturnTo();

    StringBuilder sb = new StringBuilder();
    // Expect artificial call: ReturnTo.toString()
    sb.inlinedAsSetter(returnTo);

    // Expect issue
    Origin.sink(sb);
  }

  static void testReturnToReturn() {
    ReturnTo returnTo = new ReturnTo();

    StringBuilder sb = new StringBuilder();
    // Expect artificial call: ReturnTo.toString()
    Object o = sb.get(returnTo);

    // Expect issue
    Origin.sink(o);

    // Expect no issue
    Origin.sink(sb);
  }

  static void testReturnToThisReflection() {
    StringBuilder sb = new StringBuilder();

    // Expect artificial call: ReturnTo.toString()
    sb.append(ReturnTo.class);

    // Expect issue
    Origin.sink(sb);
  }

  static void testReturnToReturnReflection() {
    StringBuilder sb = new StringBuilder();

    // Expect artificial call: ReturnTo.toString()
    Object o = sb.get(ReturnTo.class);

    // Expect issue
    Origin.sink(o);

    // Expect no issue
    Origin.sink(sb);
  }
}
