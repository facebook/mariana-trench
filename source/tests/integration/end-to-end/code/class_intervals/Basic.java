/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Basic {
  static Object staticSource() {
    return Origin.source();
  }

  static void staticSink(Object argument) {
    Origin.sink(argument);
  }

  abstract class Base {

    abstract Object potentialSource();

    abstract void potentialSink(Object argument);

    abstract Object derived1Source();

    abstract void derived1Sink(Object argument);

    abstract Object derived2SourceNotViaThis();

    abstract void derived2SinkNotViaThis(Object argument);

    Object baseSource() {
      return Origin.source();
    }

    void baseSink(Object argument) {
      Origin.sink(argument);
    }

    Object baseSourceNotViaThis() {
      return Basic.staticSource();
    }

    void baseSinkNotViaThis(Object argument) {
      Basic.staticSink(argument);
    }

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

    void truePositive2() {
      // The leaf source/sink are not class-specific. However, the flow is and
      // class intervals are expected to reflect that of Derived2's.
      Object o = this.derived2SourceNotViaThis();
      this.derived2SinkNotViaThis(o);
    }

    void truePositive3() {
      Object o = this.baseSource();
      this.baseSink(o);
    }

    void truePositive4() {
      Object o = this.baseSourceNotViaThis();
      this.baseSinkNotViaThis(o);
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
      // Specifically, we want preserves_type_context=True.
      // Making this an origin frame achieves that.
      return Origin.source();
    }

    @Override
    public void derived1Sink(Object argument) {
      Origin.sink(argument);
    }

    @Override
    public Object derived2SourceNotViaThis() {
      return new Object();
    }

    @Override
    public void derived2SinkNotViaThis(Object argument) {}

    void truePositive() {
      // Flows via base. However, as this is called from Derived1, intervals
      // should be that of Derived1's.
      Object o = this.baseSource();
      this.baseSink(o);
    }

    void truePositive2() {
      // Flows via base. However, as this is called from Derived1, intervals
      // should be that of Derived1's.
      Object o = this.baseSourceNotViaThis();
      this.baseSinkNotViaThis(o);
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

    @Override
    public Object derived2SourceNotViaThis() {
      // Specifically, we want preserves_type_context=False.
      // Calling into a non-this.* method achieves that.
      return Basic.staticSource();
    }

    @Override
    public void derived2SinkNotViaThis(Object argument) {
      Basic.staticSink(argument);
    }
  }
}
