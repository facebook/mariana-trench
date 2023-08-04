/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class PropagateIntervals {
  // Tests that propagated intervals are specific to the derived class and its source/sink kind.
  abstract class Base {
    abstract Object source();

    abstract void sink(Object argument);

    Object sourcePropagated() {
      return this.source();
    }

    void sinkPropagated(Object argument) {
      this.sink(argument);
    }
  }

  class Derived1 extends Base {
    @Override
    public Object source() {
      return Origin.source();
    }

    @Override
    public void sink(Object argument) {
      Origin.sink(argument);
    }
  }

  class Derived2 extends Base {
    Object derived2Source() {
      return new Object();
    }

    void derived2Sink(Object argument) {}

    @Override
    public Object source() {
      return this.derived2Source();
    }

    @Override
    public void sink(Object argument) {
      this.derived2Sink(argument);
    }
  }

  // Tests that when calling into a method implemented in the base class, only the derived class'
  // interval is applied. Here, source|sinkPropagated() are implemented in Base, but only Derived1's
  // source/sink are reachable. This verifies that the receiver's class intervals are applied
  // correctly even for non-this.* calls.
  class Unrelated {
    Object derived1SourceOnly(Derived1 d1) {
      return d1.sourcePropagated();
    }

    void derived1SinkOnly(Derived1 d1, Object argument) {
      d1.sinkPropagated(argument);
    }
  }
}
