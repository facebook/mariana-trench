// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import java.util.Random;

public class Flow {
  private class Tree {
    public Tree a;
    public Tree b;
    public Tree c;

    public void propagate(Tree tree) {
      Random rand = new Random();
      if (rand.nextInt(2) == 0) {
        this.a = tree.b;
      } else {
        this.a = tree.c;
      }
      // Inferred propagation: Argument(1) -> Argument(0).a
    }
  }

  public void issue() {
    Tree input = new Tree();
    input.b = (Tree) Origin.source();
    // input -> AbstractTree{`b` -> {Source()}}

    Tree output = new Tree();
    output.propagate(input);
    // Without collapsing (unsound): output -> AbstractTree{`a` -> `b` -> {Source()}}
    // With collapsing: output -> AbstractTree{`a` -> {Source()}}

    Origin.sink(output.a.c);
  }
}
