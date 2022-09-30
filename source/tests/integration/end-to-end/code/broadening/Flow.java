/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  private class Test {
    public Object tainted;
  }

  public Object broadened_issues() {
    Test test = new Test();
    test.tainted = Origin.source();
    // Collapse source flows into sink
    Origin.sink(test);

    // Collapsed source flows into return sink
    return test;
  }
}
