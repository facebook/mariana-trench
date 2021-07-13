/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  public interface A {
    void sink(Object x);
  }

  public abstract class B implements A {}

  public class C extends B {
    public void sink(Object x) {
      Origin.sink(x);
    }
  }

  public class D implements A {
    public void sink(Object x) {
      Origin.sink(x);
    }
  }

  public static void flow(B b) {
    Object x = Origin.source();
    b.sink(x);
  }
}
