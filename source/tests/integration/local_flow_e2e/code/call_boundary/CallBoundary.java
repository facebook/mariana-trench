/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

class CallBoundaryBase {
  public Object echo(Object value) {
    return value;
  }
}

public class CallBoundary extends CallBoundaryBase {
  private Object stored;

  public CallBoundary(Object initial) {
    this.stored = initial;
  }

  public void set(Object value) {
    this.stored = value;
  }

  public Object get() {
    return this.stored;
  }

  public Object own(Object value) {
    return value;
  }

  private Object hidden(Object value) {
    return value;
  }

  public static Object staticEcho(Object value) {
    return value;
  }

  public static void sink(Object value) {}

  // Explicit argument of an instance call.
  public static void instanceArgument(CallBoundary target, Object value) {
    sink(target.own(value));
  }

  // Inherited method, reached through javac's bridge and a super call.
  public static void inheritedCall(CallBoundary target, Object value) {
    sink(target.echo(value));
  }

  // Receiver state written by one call and read by the next.
  public static void setThenGet(CallBoundary target, Object value) {
    target.set(value);
    sink(target.get());
  }

  // Constructor argument stored in a field after the super constructor call.
  public static void constructorArgument(Object value) {
    sink(new CallBoundary(value).get());
  }

  // Super call resolved to the declaring class.
  public void superCall(Object value) {
    sink(super.echo(value));
  }

  // Private call on a receiver other than `this`.
  public static void privateOnOther(CallBoundary other, Object value) {
    sink(other.hidden(value));
  }

  // Static callee.
  public static void staticCall(Object value) {
    sink(staticEcho(value));
  }

  // Value passed straight to the sink.
  public static void direct(Object value) {
    sink(value);
  }
}
