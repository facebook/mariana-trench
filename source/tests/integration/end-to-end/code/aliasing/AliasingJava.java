/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class AliasingJava {
  private static class Tree {
    public Tree foo;
    public Tree bar;
    public Tree baz;
    public Tree biz;
    public Tree buzz;
  }

  private static Tree source1() {
    return new Tree();
  }

  private static Tree source2() {
    return new Tree();
  }

  private static Tree source3() {
    return new Tree();
  }

  void testSimple1() {
    Tree input = new Tree();
    input.bar = source1();

    Tree output = new Tree();
    output.foo = input;

    input.baz = source2();

    // output.foo to alias input.
    // Expect 2 issues with Source1 and Source2.
    // FN for Source2
    Origin.sink(output.foo);
  }

  void testSimple2() {
    Tree input = new Tree();
    input.bar = source1();

    Tree output = new Tree();
    output.foo = input.bar;

    input.bar.foo = source2();
    input.bar = source3();

    // output.foo aliases input.bar.
    // Update to input.bar.foo should be visible to output.foo.foo
    // output.foo == Source1, output.foo.foo == Source2.
    // Update to input.bar assigns it to a new memory-location for source3() and is a strong update.
    // This breaks the aliasing with output.foo, which still points to the old memory-location.
    // Expect 2 issues for Source1 and Source2.
    Origin.sink(output.foo.foo);

    // input.bar = source3() is a strong update.
    // Expect 1 issue only for Source3
    Origin.sink(input.bar);
  }
}
