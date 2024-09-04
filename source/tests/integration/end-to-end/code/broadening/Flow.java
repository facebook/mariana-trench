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
    public Object unrelated;
  }

  public Object broadened_issues() {
    Test test = new Test();
    test.tainted = Origin.source();
    // Collapse source flows into sink
    Origin.sink(test);

    return test;
  }

  public Test propagate(Test test) {
    return test;
  }

  public Object broadened_source() {
    Test test = new Test();
    test.tainted = Origin.source();
    return propagate(test);
  }

  public void broadened_issue_propagation() {
    Test test = new Test();
    test.tainted = Origin.source();
    test = propagate(test);
    Origin.sink(test.unrelated);
  }

  private class Tree {
    public Tree a;
    public Tree b;
    public Tree c;
    public Tree d;
    public Tree e;
  }

  public Tree widened_source_sink_depth(Tree argument) {
    Origin.sink(argument.a.b.c.d.e);
    Tree tree = new Tree();
    tree.a.b.c.d.e = (Tree) Origin.source();
    return tree;
  }
}
