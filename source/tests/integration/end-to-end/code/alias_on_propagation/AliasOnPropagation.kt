/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests

/**
 * Test cases covering flow of taint through anonymous classes in Kotlin, typically achieved through
 * lambdas. Coroutines are another common source of anonymous classes, but the coroutine libraries
 * are not mocked in the integration tests.
 */
class AliasOnPropagation {
  class Ref {
    var intValue: Int = 0
  }

  class Test {
    var ref: Ref = Ref()

    constructor() {}

    // Reference capturing constructor:
    // Inferred propagation: argument(1) -> argument(0).ref
    // User configured "alias-memory-location-on-propagation".
    constructor(inputRef: Ref) {
      ref = inputRef
    }

    // "Copy" constructor:
    // Inferred propagation: argument(1).ref -> argument(0).ref
    // User-configured "alias-memory-location-on-propagation".
    constructor(test: Test) {
      ref = test.ref
    }

    // Generation on "argument(0).ref.intValue".
    fun taintRef() = Unit
  }

  /**
   * Basic test case for "alias-memory-location-on-propagation" mode. Simulates a flow similar to
   * lambda captures (see KotlinAnonymousFunction.kt) but uses explicitly defined functions.
   * Currently a false negative until better aliasing is implemented.
   */
  fun issueFalseNegative() {
    val originalRef = Ref()
    val alias = Test(originalRef)
    alias.taintRef()
    Origin.sink(originalRef.intValue)
  }

  /**
   * The existing aliasing implementation does not support tracking MemLoc(v1.ref) = <something>.
   * This would be a false negative as a result.
   */
  fun taintOriginalReadAliasFalseNegative() {
    //   v0 = Test()  -- Instruction#0
    val original = Test()

    //   v1 = Test(v0)   -- Instruction#1
    // In practice:   MemLoc(v1).ref      <-points-to-> MemLoc(v0).ref
    // Specificially: (Instruction#1).ref <-points-to-> (Instruction#0).ref
    //
    // Interally: Cannot easily support MemLoc(v1).ref = <something> yet.
    val alias = Test(original)

    // MemLoc(v1).ref.intValue --> Source
    // (Intruction#1).ref.intValue --tainted-by--> Source
    alias.taintRef()

    //   v2 = v0.ref
    // MemLocs(v2) <-- MemLocs(v0).ref = { (Instruction#0).ref }
    //
    //   Origin.sink(v2)
    // MemLocs(v2) --tainted-by--> Sink
    // (Instruction#0).ref -points-to-> (Instruction#1).ref --tainted-by--> Source
    Origin.sink(original.ref)
  }

  /**
   * Similar to `taintOriginalReadAliasFalseNegative()` but the other way around. Verifies that
   * aliasing should work both ways. Cannot easily support bi-directional aliasing yet, hence the
   * false negative.
   */
  fun taintAliasReadOriginalFalseNegative() {
    val original = Test()
    val alias = Test(original)
    original.taintRef()
    Origin.sink(alias.ref)
  }

  /**
   * This test demonstrates the limitations of the existing aliasing implementation which tracks
   * only Register -> MemoryLocation. It should also track MemoryLocation -> MemoryLocation
   */
  fun falseNegative() {
    //   v0 = new Test() -- Instruction#0
    val original = Test()

    //   v2 = v0.ref
    //   v1 = new Test(v2)  -- Instruction#1
    // MemLocs(v2) = { (Instruction#0).ref, (Instruction#1).ref }
    val alias: Test = Test(original.ref)

    //   invoke-virtual v1 taintRef()
    // MemLocs(v1).ref = (Instruction#1).ref --> Source
    alias.taintRef()

    //   v2 = v0.ref
    // MemLocs(v2) previously held information for:
    // (Instruction#1).ref <-points-to-> (Instruction#0).ref
    //
    // Assignment results in: MemLocs(v2) = { (Instruction#0).ref } -/-> Source
    // A separate MemoryLocation -> MemoryLocation mpa is needed to maintain aliasing information.
    //
    // False negative as the source is not seen by the analysis:
    //   v2 = Integer.valueOf(v2.intValue)
    //   invoke-static v2 Origin;.sink(Object)V
    Origin.sink(original.ref.intValue)
  }
}
