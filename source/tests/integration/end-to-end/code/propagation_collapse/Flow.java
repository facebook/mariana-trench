/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import java.util.Random;

public class Flow {
  private class Tree {
    public Tree a;
    public Tree b;
    public Tree c;
    public Tree d;
    public Tree e;
    public Tree f;

    public void propagate(Tree tree) {
      Random rand = new Random();
      if (rand.nextInt(2) == 0) {
        this.a = tree.b;
      } else if (rand.nextInt(2) == 1) {
        this.a = tree.c;
      } else if (rand.nextInt(2) == 2) {
        this.a = tree.d;
      } else if (rand.nextInt(2) == 3) {
        this.a = tree.e;
      } else if (rand.nextInt(2) == 4) {
        this.a = tree.f;
      }
      // Inferred propagation: Argument(1) -> Argument(0).a as the input paths are collapsed when it
      // exceeds Heuristics::propagation_max_input_path_leaves().
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
