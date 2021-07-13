/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  static Object return_sink(Object x) {
    Object y = propagate(x);
    return y;
  }

  static Object direct_issue() {
    Object x = Origin.source();
    Object y = propagate(x);
    return y;
  }

  static void indirect_issue() {
    Object x = Origin.source();
    Object y = propagate(x);
    return_sink(y);
  }

  static Object propagate(Object x) {
    Object[] array = new Object[2];
    array[0] = x;
    return array[0];
  }

  private static class Tree {
    public Tree foo;
    public Tree bar;
  };

  static Tree return_sink_foo(Tree tree) {
    return tree;
  }

  static Tree direct_issue_foo() {
    Tree tree = new Tree();
    tree.foo = (Tree) Origin.source();
    return tree;
  }

  static Tree direct_non_issue_foo() {
    Tree tree = new Tree();
    tree.bar = (Tree) Origin.source();
    return tree;
  }

  static void indirect_issue_foo() {
    Tree tree = new Tree();
    tree.foo = (Tree) Origin.source();
    return_sink_foo(tree);
  }

  static void indirect_non_issue_foo() {
    Tree tree = new Tree();
    tree.bar = (Tree) Origin.source();
    return_sink_foo(tree);
  }
}
