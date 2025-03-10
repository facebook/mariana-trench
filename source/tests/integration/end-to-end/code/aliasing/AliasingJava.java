/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import java.util.Random;

public class AliasingJava {
  private static class Tree {
    public Tree foo;
    public Tree bar;
    public Tree baz;
    public Tree biz;

    public void generationOnFoo() {}
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

  private static void generationOnArg(Tree arg) {}

  private static void generationOnArgFooBar(Tree arg) {}

  private static void generationOnArgBar(Tree arg) {}

  static void testSimple1() {
    Tree input = new Tree();
    input.bar = source1();
    Tree output = new Tree();
    // move-object v1, v2
    output.foo = input;

    input.baz = source2();

    // output.foo to alias input.
    // Expect 2 issues with Source1 and Source2.
    Origin.sink(output.foo);
  }

  static void testSimple2() {
    Tree input = new Tree();
    // $input -> MemLoc(new-instance Tree @ input) = {taint={}, aliases={}}

    input.bar = source1();
    // $source1 -> MemLoc(invoke-static source1()) = {taint={Source1}, aliases={}}
    // $input.bar -> FieldMemLoc(MemLoc(new-instance Tree @ input), bar)
    // $input -> MemLoc(new-instance Tree @ input) = {
    //            taint={},
    //            aliases={.bar = MemLoc(invoke-static source1())}}

    Tree output = new Tree();
    // $output -> MemLoc(new-instance Tree @ output) = {taint={}, aliases={}}

    output.foo = input.bar;
    // $output.foo -> FieldMemLoc(MemLoc(new-instance Tree @ output), foo)
    // $output  -> MemLoc(new-instance Tree @ output) = {
    //        taint={},
    //        aliases={.foo = {MemLoc(invoke-static source1())}}

    input.bar.baz = source2();
    // $source2 -> MemLoc(invoke-static source2()) = {taint={Source2}, aliases={}}
    // $input.bar.baz -> FieldMemLoc(FieldMemLoc(MemLoc(new-instance Tree @ input), bar), baz)
    // $input -> MemLoc(new-instance Tree @ input) = {
    //            taint={},
    //            aliases={.bar = MemLoc(invoke-static source1())}} // remaining path = .baz
    // $source1 -> MemLoc(invoke-static source1()) = {
    //          taint = {Source1},
    //          aliases = {.baz = {MemLoc(invoke-static source2())}}}

    input.bar = source3();
    // $source3 -> MemLoc(invoke-static source3()) = {taint={Source3}, aliases={}}
    // $input.bar -> FieldMemLoc(MemLoc(new-instance Tree @ input), bar)
    // $input -> MemLoc(new-instance Tree @ input) = {
    //            taint={},
    //            aliases={.bar = MemLoc(invoke-static source3())}} // Strong write

    // output.foo aliases input.bar.
    // Update to input.bar.baz should be visible to output.foo.baz
    // output.foo == Source1, output.foo.baz == Source2.
    // Update to input.bar assigns it to a new memory-location for source3() and is a strong update.
    // This breaks the aliasing with output.foo, which still points to the old memory-location.
    // Expect 2 issues for Source1 and Source2.
    Origin.sink(output.foo.baz);
    // $output.foo.baz -> FieldMemLoc(FieldMemLoc(MemLoc(new-instance Tree @ output), foo), baz)
    // Resolve aliases -> aliases={{MemLoc(new-instance Tree @ output)}
    //    .foo = {MemLoc(invoke-static source1())}
    //        .baz = {MemLoc(invoke-static source2())}
    // }
    // Deep read taint tree -> taint={{}
    //    .foo = {Source1},
    //        .baz = {Source2}
    // }
    // Issue with Source1 and Source2

    // input.bar = source3() is a strong update.
    // Expect 1 issue only for Source3
    Origin.sink(input.bar);
    // Result state:
    // $input.bar -> FieldMemLoc(MemLoc(new-instance Tree @ input), bar)
    // $input -> MemLoc(new-instance Tree @ input) = {
    //            taint={},
    //            aliases={.bar = MemLoc(invoke-static source3())}}
    // Resolve aliases -> aliases={{MemLoc(new-instance Tree @ input)}
    //    .bar = {MemLoc(invoke-static source3())}
    // }
    // Deep read taint tree -> taint={{}
    //    .bar = {Source3},
    // }
    // Issue with Source3.
  }

  static void testWeakUpdateForAlias() {
    Tree input = new Tree();
    input.bar = source1();
    input.baz = source2();

    Tree other = new Tree();
    Tree output = new Tree();

    if (new Random().nextInt(2) == 0) {
      output.bar = input.bar;
    } else {
      output.bar = input.baz;
    }

    output.foo = other;

    // Update through alias.
    other.biz = source3();

    // Only expect 1 issue for Source3.
    Origin.sink(output.foo.biz);

    // Break alias.
    input.bar = source3();
    // Expect issues for Source1 and Source2
    Origin.sink(output.bar);
    // Expect issue for Source3 only.
    Origin.sink(input.bar);
  }

  static void testAliasCycle() {
    Tree input = new Tree();
    input.foo = source1();
    input.bar = source2();

    Tree output = new Tree();
    output.foo = input;

    // Creates a cycle here:
    // output.foo -> input -> .baz -> output
    //   ^______________________________|
    // output.foo.baz --+
    //   ^______________|
    input.baz = output;

    // Expect issues for Source1 and Source2.
    Origin.sink(output.foo);

    output.bar = source3();
    // Expect issues with Source3.
    // input.baz -> output -> .bar = Source3
    Origin.sink(input.baz.bar);

    // Expect issues for Source1, Source2, and Source3 due to widening on aliasing cycles.
    // The points-to tree is widened and has the collapse depth set to 0 at the head:
    // Widened PointsToTree({
    //  [(input + output):collapse-depth=0] -> {
    //      .foo -> {source1, source3},
    //      .bar -> {source2} }
    // })
    // Deep read taint tree at root memory location of output is collapsed to:
    // TaintTree({Source1, Source2, Source3})
    Origin.sink(output.foo.baz);
  }

  static void testDifferentRootMemoryLocationsAtDifferentDepths() {
    Tree input1 = new Tree();
    input1.foo = source1();

    Tree input2 = new Tree();
    input2.foo = source2();

    Tree output = new Tree();
    if (new Random().nextInt(2) == 2) {
      // Read from non-existent path in the points-to tree.
      output.foo = input1.foo.biz;
    } else {
      // Write to path where intermediate node does not exist in the points-to tree.
      output.foo.bar = input2;
    }

    // Create alias
    output.bar = input1;
    // Update using aliasing memory location
    output.bar.baz = source3();
    // Update using aliased memory location
    input1.foo.bar = source3();

    // Expect 1 issue for Source2.
    // output.foo.bar -> input2 -> .foo = Source2
    //
    // 1 FN issue for Source1.
    // We have: output.foo = input1.foo.biz;
    // Here, $input.foo.biz -> FieldMemLoc(FieldMemLoc(MemLoc(new-instance Tree @ input), foo), biz)
    // which is _|_ in the points-to tree.
    // So, points-to tree for $output does not track any aliasing information to the $input tree.
    // But, since $input1.foo is tainted with Source1, a read from $input1.foo.biz also results in
    // Source1 as taint is propagated down. But we currently miss this issue.
    Origin.sink(output.foo.bar);

    // Expect 1 issue for Source3.
    // output.bar -> input1.baz -> _|_
    //       .bar.baz = Source3
    Origin.sink(output.bar.baz);

    // Expect 2 issues for Source1 and Source3.
    // output.bar -> input1 -> .foo = Source1
    //                         .foo.bar = Source3
    Origin.sink(output.bar.foo.bar);
  }

  static void testGenerationOnThisFooBeforeAlias() {
    Tree input = new Tree();
    input.generationOnFoo();

    Tree output = new Tree();
    output.bar = input.foo;

    // Expect 1 issue for Source4.
    Origin.sink(output.bar);

    output.bar.baz = source1();
    // Expect 2 issues for Source1 and Source4.
    Origin.sink(input.foo.baz);
  }

  static void testGenerationOnArgBeforeAlias(Tree parameter, boolean condition) {
    if (condition) {
      generationOnArg(parameter.foo.bar);
    } else {
      parameter.foo = new Tree();
    }

    Tree x = new Tree();
    x.foo = parameter.foo.bar;

    Origin.sink(x);
  }

  static void testGenerationOnArgAfterAlias(Tree parameter, boolean condition) {
    Tree tmp = new Tree();

    if (condition) {
      parameter.foo = new Tree();
    } else {
      tmp = parameter.foo;
    }

    Tree x = new Tree();
    x.foo = parameter.foo;
    if (condition) {
      generationOnArg(tmp.bar);
    }

    Origin.sink(x.foo);
  }

  static void testGenerationOnArgFooBarBeforeAlias(Tree parameter, boolean condition) {
    if (condition) {
      generationOnArgFooBar(parameter);
    } else {
      parameter.foo = new Tree();
    }

    Tree x = new Tree();
    x.foo = parameter.foo.bar;

    Origin.sink(x);
  }

  static void testGenerationOnArgBarAfterAlias(Tree parameter, boolean condition) {
    Tree tmp = new Tree();
    if (condition) {
      parameter.foo = new Tree();
    } else {
      tmp = parameter.foo;
    }

    Tree x = new Tree();
    x.foo = parameter.foo;
    if (condition) {
      generationOnArgBar(tmp);
    }

    Origin.sink(x.foo);
  }
}
