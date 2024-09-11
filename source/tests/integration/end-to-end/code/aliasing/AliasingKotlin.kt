/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests

import kotlin.random.Random

class Tree() {
  lateinit var foo: Tree
  lateinit var bar: Tree
  lateinit var baz: Tree
  lateinit var biz: Tree
  lateinit var buzz: Tree

  fun titoNoCollapsed(input: Tree) {
    if (Random.nextInt(2) == 0) {
      this.foo = input.foo
    } else if (Random.nextInt(2) == 1) {
      this.foo = input.bar
    } else if (Random.nextInt(2) == 2) {
      this.foo = input.baz
    } else if (Random.nextInt(2) == 3) {
      this.foo = input.biz
    }
  }

  fun titoCollapsed(input: Tree) {
    if (Random.nextInt(2) == 0) {
      this.foo = input.foo
    } else if (Random.nextInt(2) == 1) {
      this.foo = input.bar
    } else if (Random.nextInt(2) == 2) {
      this.foo = input.baz
    } else if (Random.nextInt(2) == 3) {
      this.foo = input.biz
    } else if (Random.nextInt(2) == 4) {
      this.foo = input.buzz
    }
    // Additional propagation from input.buzz to .foo compared to titoNoCollapse() exceeds the
    // default Heurisitcs::propagation_max_input_path_leaves() of 4 and leads to
    // Inferred propagation: Argument(1) -> Argument(0).foo with collapsed input paths
  }

  fun generationSource4() = Unit
}

class AliasingKotlin {
  fun source1(): Tree {
    return Tree()
  }

  fun source2(): Tree {
    return Tree()
  }

  fun source3(): Tree {
    return Tree()
  }

  fun testSimple1() {
    val input = Tree()
    input.bar = source1()

    val output = Tree()
    output.foo = input

    input.baz = source2()

    // output.foo to aliases input.
    // Expect 2 issues with Source1 and Source2.
    // FN for Source2
    Origin.sink(output.foo)
  }

  fun testSimple2() {
    val input = Tree()
    input.bar = source1()

    val output = Tree()
    output.foo = input.bar

    input.bar.foo = source2()
    input.bar = source3()

    // output.foo aliases input.bar.
    // Update to input.bar.foo should be visible to output.foo.foo
    // output.foo == Source1, output.foo.foo == Source2.
    // Update to input.bar assigns it to a new memory-location for source3() and is a strong update.
    // This breaks the aliasing with output.foo, which still points to the old memory-location.
    // Expect 2 issues for Source1 and Source2.
    Origin.sink(output.foo.foo)

    // input.bar = source3() is a strong update.
    // Expect 1 issue only for Source3
    Origin.sink(input.bar)
  }

  fun testSimpleTaintTreeGeneration() {
    val input = Tree()
    var output = Tree()
    output = input.foo
    input.foo.generationSource4()

    // Expect issue for Source4
    Origin.sink(input.foo)
    // Expect issue for Source4
    Origin.sink(output)
  }

  fun testNoCollapsePropagations() {
    val input = Tree()

    input.bar = source1()
    input.baz = source2()

    val outputNoCollapse = Tree()
    // outputNoCollapse.foo = {input.foo, input.bar, input.baz, input.biz}
    outputNoCollapse.titoNoCollapsed(input)

    // Test aliasing for existing taint.
    // Expect issues with Source1 and Source2
    Origin.sink(outputNoCollapse.foo)

    // Test aliasing for update through original variable.
    input.buzz.biz = source3()
    // Expect issues with Source1 and Source2 only. No new issue for source3 as no propagation from
    // input.buzz.
    Origin.sink(outputNoCollapse.foo.biz)

    input.foo.generationSource4()
    // Expect new issue for Source4 + existing issues for Source1 and Source2.
    Origin.sink(outputNoCollapse.foo)

    // Test strong update on original variable being aliased.
    input.foo = source1()
    // Expect 1 issue for Source1
    Origin.sink(input.foo)
    // Expect 3 issues for Source1, Source2, and Source
    Origin.sink(outputNoCollapse.foo)
  }

  fun testCollapsePropagations() {
    val input = Tree()

    input.bar = source1()
    input.baz = source2()

    val outputCollapse = Tree()
    // Propagation from input to outputCollapse.foo
    // To test two options:
    // 1. Resolve aliases to all children of input in the PointsToTree.
    // 2. Unresolved alias directly to input.
    outputCollapse.titoCollapsed(input)

    // Test aliasing for existing taint.
    // Expect issues with Source1 and Source2
    Origin.sink(outputCollapse.foo)

    // Test aliasing for update through original variable.
    input.buzz.biz = source3()
    // TP would be issues for Source1, Source2, and Source3.
    // Expected output will depend on how we track aliases for collapsed propagations.
    Origin.sink(outputCollapse.foo.biz)

    input.foo.generationSource4()
    // TP would be new issue for Source4.
    Origin.sink(outputCollapse.foo)

    // Test strong update on original variable being aliased.
    input.foo = source1()
    // Expect 1 issue for Source1
    Origin.sink(input.foo)
    // TP would be to not have an issue for Source4.
    Origin.sink(outputCollapse.foo)
  }

  fun testConditionalAliasing(argument: Tree) {
    val input = Tree()
    input.foo = source1()
    input.bar = source2()

    var output = Tree()
    if (Random.nextInt(2) == 0) {
      output = input.foo
    } else {
      output = input.bar
    }

    // Test update through aliased variable.
    // output aliases both input.foo and input.bar.
    // With this assignment:
    // - input.foo.baz _may_ point to previous memory-location (source1()) Or argument
    // - input.bar.baz _may_ point to previous memory-location (source2()) Or argument
    output.baz = argument

    // Test update through original variable.
    argument.biz = source3()

    // Expect 2 issues for Source1 and Source3
    Origin.sink(input.foo.biz)

    // Expect 2 issues for Source2 and Source3
    Origin.sink(input.bar.biz)
  }
}
