/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class MultiSourcesSingleArg {
  public static Object sourceA() {
    return new Object();
  }

  public static void sink(MockObjectWithThisPropagation intent) {}

  public void issue() {
    MockObjectWithThisPropagation intent = new MockObjectWithThisPropagation();
    intent.setField(MultiSourcesSingleArg.sourceA());
    MultiSourcesSingleArg.sink(intent);
  }

  public void triggeredSinkB(MockObjectWithThisPropagation intent) {
    intent.setField(MultiSourcesSingleArg.sourceA());
    MultiSourcesSingleArg.sink(intent);
  }

  public void triggeredSinkA(Object obj) {
    // Note: new MockObjectWithThisPropagation() is sink "b".
    MockObjectWithThisPropagation intent = new MockObjectWithThisPropagation();
    intent.setField(obj);
    MultiSourcesSingleArg.sink(intent);
  }
}
