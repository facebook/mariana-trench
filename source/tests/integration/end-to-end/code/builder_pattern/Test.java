/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Test {
  public Test x;
  public Test y;
  public Test z;

  public Test() {}

  public Test(Test x, Test y, Test z) {
    this.x = x;
    this.y = y;
    this.z = z;
  }

  // Method marked as obscure in models.json
  public Test obscure() {
    return new Test();
  }

  public static final class Builder {
    public Test x;
    public Test y;
    public Test z;

    public Builder setX(Test x) {
      this.x = x;
      return this;
    }

    public Builder setY(Test y) {
      this.y = y;
      return this;
    }

    public Builder setZ(Test z) {
      this.z = z;
      return this;
    }

    public Builder setCollapseX(Test x) {
      this.x = x.obscure();
      return this;
    }

    public Builder setCollapseY(Test y) {
      this.y = y.obscure();
      return this;
    }

    public Builder setCollapseZ(Test z) {
      this.z = z.obscure();
      return this;
    }

    public Test build() {
      return new Test(this.x, this.y, this.z);
    }
  }

  public static Test testPerfectPropagation(Test x, Test y, Test z) {
    return (new Builder()).setX(x).setY(y).setZ(z).build();
  }

  public static void testBuilderPreservesStructure() {
    Test input = new Test();
    input.x = (Test) Origin.source();

    Test output = (new Builder()).setX(null).setY(input).setZ(null).build();

    // Not an issue.
    Origin.sink(output.x);
    Origin.sink(output.z);

    // Issue.
    Origin.sink(output.y.x);

    // Not an issue.
    Origin.sink(output.y.z);

    // Issue because we collapse memory location for output.y.y into output.y to avoid blowing up.
    Origin.sink(output.y.y);

    // Issue because of issue broadening.
    Origin.sink(output.y);
  }

  public static void testBuilderNonPreservesStructure() {
    Test input = new Test();
    input.x = (Test) Origin.source();

    Test output = (new Builder()).setCollapseX(null).setCollapseY(input).setCollapseZ(null).build();

    // Not an issue.
    Origin.sink(output.x);
    Origin.sink(output.z);

    // Issue.
    Origin.sink(output.y.x);

    // TODO(T158150113): Should be an issue because taint is collapsed in `setCollapseY`
    Origin.sink(output.y.z);

    // Issue.
    Origin.sink(output.y.y);

    // Issue because of issue broadening.
    Origin.sink(output.y);
  }

  public static void testBuilderMultipleSetSink(Test first, Test second) {
    // `first` leads to a sink.
    Origin.sink((new Builder()).setX(null).setX(first).build());

    // TODO(T156926106): `second` does NOT lead to a sink, but we have a false positive.
    Origin.sink((new Builder()).setX(second).setX(null).build());
  }

  public static void testBuilderMultipleSetIssue() {
    // Issue
    Origin.sink((new Builder()).setX(null).setX((Test) Origin.source()).build());

    // TODO(T156926106): Should not be an issue, but false positive.
    Origin.sink((new Builder()).setX((Test) Origin.source()).setX(null).build());
  }

  public static void testBuilderOverlapping() {
    Test source_on_x = new Test();
    source_on_x.x = (Test) Origin.source();

    Test source_on_y = new Test();
    source_on_y.y = (Test) Origin.source();

    Test output = (new Builder()).setZ(source_on_x).setZ(source_on_y).build();

    // TODO(T156926106): Should not be an issue, but false positive.
    Origin.sink(output.z.x);

    // Issue.
    Origin.sink(output.z.y);
  }
}
