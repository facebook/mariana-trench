/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {

  public Object foo;

  private class Test {
    public Object field1;
    public Object field2;
  }

  public void inferred_sink(Test argument) {
    argument.field1 = new Object();
    // Do not infer propagations from Arg(1).field to the sink
    Origin.sink(argument.field1);

    argument.field2 = Origin.source();
    Origin.sink(argument.field2);
  }

  public Object inferred_propagation(Test argument) {
    argument.field1 = new Object();

    // Do not infer propagations from Arg(1).field to Arg(0).foo or Return
    this.foo = argument.field1;
    return argument.field1;
  }
}
