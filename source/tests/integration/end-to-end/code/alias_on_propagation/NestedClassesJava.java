/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class NestedClassesJava {
  public Object mTopLevel;

  class FirstLevel {
    public Object mFirstLevel;

    // implicit constructor <init>(NestedClassesJava arg) {
    //   this.this$n = arg;
    // }
    // Expect inferred propagation: Argument(1) -> Argument(0).this$n

    class SecondLevel {
      Object mSecondLevel;

      // implicit constructor <init>(FirstLevel arg) {
      //   this.this$n = arg;
      // }
      // Expect inferred propagation: Argument(1) -> Argument(0).this$n

      void secondLevelSinkOnFirstLevelMember() {
        // Same as directly accessing mFirstLevel (unless shadowing).
        // Explicitly specifying for clarity.
        Origin.sink(FirstLevel.this.mFirstLevel);
      }

      void secondLevelSinkOnTopLevelMember() {
        // Same as directly accessing mTopLevel (unless shadowing).
        // Explicitly specifying for clarity.
        // Expect inferred sink on: Argument(0).this$1.this$0.mTopLevel
        Origin.sink(NestedClassesJava.this.mTopLevel);
      }
    }

    // ------------------------------------
    //  FirstLevel Methods
    // ------------------------------------

    void firstLevelSinkOnTopLevelMember() {
      // Same as directly accessing mTopLevel (unless shadowing).
      // Explicitly specifying for clarity.
      // Expect inferred sink on Argument(0).this$1.mTopLevel
      Origin.sink(NestedClassesJava.this.mTopLevel);
    }
  }

  // ------------------------------------
  //  Top Level Methods
  // ------------------------------------

  void wrapperWithArgFirstLevelSinkOnTopLevelMember(FirstLevel arg) {
    // Expect inferred sink on Argument(1).this$n.mTopLevel
    arg.firstLevelSinkOnTopLevelMember();
  }

  void wrapperFirstLevelSinkOnTopLevelMember() {
    // Expect inferred sink on Argument(0).mTopLevel
    //
    // The propagation from the implicit argument of the constructor results in
    // correct resolution of access path `.this$n`
    // Propagation: Argument(1) -> Argument(0).this$n
    FirstLevel firstLevel = new FirstLevel();

    // Has sink on: Argument(0).this$1.mTopLevel
    firstLevel.firstLevelSinkOnTopLevelMember();
  }

  void testIssueWrapperFirstLevelSinkOnTopLevelMember() {
    FirstLevel firstLevel = new FirstLevel();
    // Propagation: Argument(1) -> Argument(0).this$n

    mTopLevel = Origin.source();
    // Generation: Argument(0).mTopLevel

    firstLevel.firstLevelSinkOnTopLevelMember();
    // Sink: Argument(0).this$1.mTopLevel
    // FN: No issue found.
    // Note: Correctly infer source and sink on the same port:
    // Argument(0).mTopLevel
    // So we are reading the taint incorrectly when we check for flows.
    // Forward analyze_invoke() -> check_flows()
    // - invoke v0, FirstLevel;.firstLevelSinkOnTopLevelMember()
    //   where v0 -> MemoryLocation(new-instance FirstLevel;)
    // - To correctly resolve the deep read when we check for flows for this invoke,
    // we need to apply the aliasing propagation on the constructor:
    //   Argument(1) -> Argument(0).this$n
  }

  void wrapperWithArgSecondLevelSinkOnFirstLevelMember(FirstLevel.SecondLevel arg) {
    // Expect inferred sink on Argument(1).this$1.mTopLevel
    arg.secondLevelSinkOnFirstLevelMember();
  }

  void wrapperWithArgSecondLevelSinkOnTopLevelMember(FirstLevel.SecondLevel arg) {
    // Expect inferred sink on Argument(1).this$1.this$0.mTopLevel
    arg.secondLevelSinkOnTopLevelMember();
  }
}
