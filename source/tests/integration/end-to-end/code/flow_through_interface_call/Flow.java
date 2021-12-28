/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
