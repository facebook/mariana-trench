/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Basic {
  abstract class Base {
    abstract Object potentialSource();

    abstract void potentialSink(Object argument);

    abstract Object derived1Source();

    abstract void derived1Sink(Object argument);

    void noIssue() {
      // Derived1's source cannot flow into Derived2's sink since they are
      // unrelated objects in the hierarchy. Class intervals are necessary
      // to address this or we will get a false positive.
      Object o = this.potentialSource();
      this.potentialSink(o);
    }

    void truePositive() {
      Object o = this.derived1Source();
      this.derived1Sink(o);
    }
  }

  class Derived1 extends Base {
    @Override
    public Object potentialSource() {
      return Origin.source();
    }

    @Override
    public void potentialSink(Object argument) {}

    @Override
    public Object derived1Source() {
      return Origin.source();
    }

    @Override
    public void derived1Sink(Object argument) {
      Origin.sink(argument);
    }
  }

  class Derived2 extends Base {
    @Override
    public Object potentialSource() {
      return new Object();
    }

    @Override
    public void potentialSink(Object argument) {
      Origin.sink(argument);
    }

    @Override
    public Object derived1Source() {
      return new Object();
    }

    @Override
    public void derived1Sink(Object argument) {}
  }
}
