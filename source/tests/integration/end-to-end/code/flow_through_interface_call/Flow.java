// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

public class Flow {
  public interface A {
    void sink(Object x);
  }

  public interface B extends A {
    void sink(Object x); // Sink from models.json
  }

  public class C implements B {
    public void sink(Object x) {}
  }

  public static void flow(A x) {
    Object y = Origin.source();
    x.sink(y);
  }
}
