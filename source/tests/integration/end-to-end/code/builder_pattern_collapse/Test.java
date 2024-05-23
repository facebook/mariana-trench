/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Test {
  // 4 fields, configure generation_max_output_path_leaves = 2
  public Test a;
  public Test b;
  public Test c;
  public Test d;

  public Test() {}

  public Test(Test a, Test b, Test c, Test d) {
    this.a = a;
    this.b = b;
    this.c = c;
    this.d = d;
  }

  private static Object differentSource() {
    return new Object();
  }

  private static void differentSink(Object obj) {}

  public static final class Builder {
    public Test a;
    public Test b;
    public Test c;
    public Test d;

    public Builder setA(Test a) {
      this.a = a;
      return this;
    }

    public Builder setB(Test b) {
      this.b = b;
      return this;
    }

    public Builder setC(Test c) {
      this.c = c;
      return this;
    }

    public Builder setD(Test d) {
      this.d = d;
      return this;
    }

    public Test build() {
      return new Test(this.a, this.b, this.c, this.d);
    }
  }

  public static Test collapseOnBuilder() {
    // Resulting taint tree will have caller port Return
    // Taint 3 fields to trigger collapsing behavior,
    // configure generation_max_output_path_leaves = 2
    return (new Builder())
        .setA((Test) Origin.source())
        .setB((Test) Test.differentSource())
        .setC((Test) Origin.source())
        .build();
  }

  public static Test noCollapseOnBuilder() {
    // Resulting taint tree will have caller port Return.a and Return.b
    return (new Builder()).setA((Test) Origin.source()).setB((Test) Test.differentSource()).build();
  }

  public static void testCollapseOnBuilder() {
    Test output = collapseOnBuilder();

    // issue: a has "Source" and b has "DifferentSource"
    Origin.sink(output.a);
    Test.differentSink(output.b);

    // Also issue: false positive due to collapsing
    Test.differentSink(output.a);
    Origin.sink(output.b);
    Origin.sink(output.d);
    Test.differentSink(output.d);
  }

  public static void testNoCollapseOnBuilder() {
    Test output = noCollapseOnBuilder();

    // issue: a has "Source" and b has "DifferentSource"
    Origin.sink(output.a);
    Test.differentSink(output.b);

    // no issue, since no collapsing occurs
    Test.differentSink(output.a);
    Origin.sink(output.b);
    Origin.sink(output.d);
    Test.differentSink(output.d);
  }
}
