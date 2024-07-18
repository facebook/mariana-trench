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

  public Test(Test.Builder builder) {
    this.a = builder.a;
    this.b = builder.b;
    this.c = builder.c;
    this.d = builder.d;
  }

  private static Object differentSource() {
    return new Object();
  }

  private static void differentSink(Object obj) {}

  public void inlineAsSetter(Test other) {
    this.a.b.c = other.c.d.a;
  }

  public Test inlineAsGetter() {
    return this.a.b.c;
  }

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

    public Test buildUsingFieldsAsArguments() {
      return new Test(this.a, this.b, this.c, this.d);
    }

    public Test buildUsingBuilderAsArgument() {
      return new Test(this);
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
        .buildUsingFieldsAsArguments();
  }

  public static Test noCollapseOnBuilder() {
    // Resulting taint tree will have caller port Return.a and Return.b
    return (new Builder())
        .setA((Test) Origin.source())
        .setB((Test) Test.differentSource())
        .buildUsingFieldsAsArguments();
  }

  public static Test noCollapseOnApproximateBuildUsingFields() {
    // Still the 3-field pattern, but with "no-collapse-on-approximate" enabled
    return (new Builder())
        .setA((Test) Origin.source())
        .setB((Test) Test.differentSource())
        .setC((Test) Origin.source())
        .buildUsingFieldsAsArguments();
  }

  public static Test noCollapseOnApproximateBuildUsingThis() {
    // Still the 3-field pattern, but with "no-collapse-on-approximate" enabled
    return (new Builder())
        .setA((Test) Origin.source())
        .setB((Test) Test.differentSource())
        .setC((Test) Origin.source())
        .buildUsingBuilderAsArgument();
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

  public static void testNoCollapseOnApproximateBuildUsingFields() {
    Test output = noCollapseOnApproximateBuildUsingFields();

    // issue: a has "Source" and b has "DifferentSource"
    Origin.sink(output.a);
    Test.differentSink(output.b);

    // no issue, since no collapsing occurs
    Test.differentSink(output.a);
    Origin.sink(output.b);
    Origin.sink(output.d);
    Test.differentSink(output.d);
  }

  public static void testNoCollapseOnApproximiateBuildUsingThis() {
    Test output = noCollapseOnApproximateBuildUsingThis();

    // issue: a has "Source" and b has "DifferentSource"
    Origin.sink(output.a);
    Test.differentSink(output.b);

    // no issue, since no collapsing occurs
    Test.differentSink(output.a);
    Origin.sink(output.b);
    Origin.sink(output.d);
    Test.differentSink(output.d);
  }

  public void testInlineAsGetter() {
    Test b = new Test();
    b.c = (Test) Origin.source(); // b.c = tainted

    Test a = new Test();
    a.b = b; // a.b.c = tainted

    Test x = new Test();
    x.a = a; // x.a.b.c = tainted

    // Expect issue as: x.a.b.c is tainted so x.a.b.c.d is also tainted.
    Origin.sink(x.a.b.c.d);
    // Expect issue with propagation_max_input_path_size = 2.
    // - inline_as_getter: Argument(0).a.b.c
    // - propagation: Argument(0).a.b -> LocalReturn (collapse depth = 0)
    // - is_safe_to_inline is false as inline_as_getter and propagation mismatch.
    // - so propagation model is applied.
    // - Since collapse depth is 0, result memory location after call to inlineAsGetter() is tainted
    //   and we find the issue.
    Origin.sink(x.inlineAsGetter().d);
  }

  public void testInlineAsSetter() {
    Test b = new Test();
    b.c.d.a = (Test) Origin.source(); // b.c.d.a = tainted

    // Expect no issues as: only b.c.d.a is tainted
    Origin.sink(b.c.d.b);

    Test x = new Test();
    x.inlineAsSetter(b); // x.a.b.c = tainted
    // Should be no issue as: only x.a.b.c is tainted
    // But FP when propagation_max_[input|output]_path_size = 2 which is the expected sound result.
    // For inlineAsSetter():
    // - inline_as_setter: value=Argument(1).c.d.a, target=Argument(0).a.b.c
    // - propagation: Argument(1).c.d -> Argument(0).a.b (collapse depth = 0)
    // - is_safe_to_inline is false as inline_as_setter and propagation mismatch.
    // - so propagation model is applied.
    // - result memory location after call to inlineAsSetter() is: .a.b = tainted
    // FP issue (as expected) as x.a.b is tainted so x.a.b.d is also tainted.
    Origin.sink(x.a.b.d);
  }
}
