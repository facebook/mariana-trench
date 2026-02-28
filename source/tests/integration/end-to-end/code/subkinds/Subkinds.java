/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Subkinds {

  // Helper methods for sources with subkinds.
  // Models declare these as returning Source(A), Source(B), etc.
  static Object getSourceA() {
    return new Object();
  }

  static Object getSourceB() {
    return new Object();
  }

  // Helper methods for sinks with subkinds.
  static void sinkA(Object o) {}

  static void sinkB(Object o) {}

  // Helper: Two-argument sink with different subkinds per argument.
  // Models declare Argument(0) as Sink(A) and Argument(1) as Sink(B).
  static void sinkAAndB(Object a, Object b) {}

  // Helper for transforms.
  static Object transform(Object o) {
    return o;
  }

  // Case 1: Baseline — plain Source -> Sink (no subkinds).
  // Expected: 1 issue (rule 1).
  void baselineFlow() {
    Object x = Origin.source();
    Origin.sink(x);
  }

  // Case 2: Source with subkind -> Sink (no subkind).
  // Source(A) matches rule 1 via discard_subkind().
  // Expected: 1 issue (rule 1).
  void sourceSubkindFlow() {
    Object x = getSourceA();
    Origin.sink(x);
  }

  // Case 3: Source (no subkind) -> Sink with subkind.
  // Sink(A) matches rule 1 via discard_subkind().
  // Expected: 1 issue (rule 1).
  void sinkSubkindFlow() {
    Object x = Origin.source();
    sinkA(x);
  }

  // Case 4: Source with subkind -> Sink with subkind.
  // Source(A) -> Sink(A), both match rule 1 via discard_subkind().
  // Expected: 1 issue (rule 1).
  void bothSubkindFlow() {
    Object x = getSourceA();
    sinkA(x);
  }

  // Case 5: Source with subkind -> Transform -> Sink.
  // Source(A) -> T1 -> Sink matches rule 2 (has transform T1).
  // Expected: 1 issue (rule 2).
  void sourceSubkindWithTransformFlow() {
    Object x = getSourceA();
    x = transform(x);
    Origin.sink(x);
  }

  // Case 6: Source -> Transform -> Sink with subkind.
  // Source -> T1 -> Sink(A) matches rule 2 (has transform T1).
  // Expected: 1 issue (rule 2).
  void sinkSubkindWithTransformFlow() {
    Object x = Origin.source();
    x = transform(x);
    sinkA(x);
  }

  // Case 7: Two source subkinds at the same sink.
  // Source(A) and Source(B) both flow to the same Origin.sink() call.
  // Expected: 1 issue (rule 1) with 2 source entries (Source(A) and Source(B)).
  void twoSourceSubkindsSameSink(boolean condition) {
    Object x;
    if (condition) {
      x = getSourceA();
    } else {
      x = getSourceB();
    }
    Origin.sink(x);
  }

  // Case 8: Two sink subkinds at the same source.
  // The same Origin.source() flows to both sinkA() and sinkB().
  // Expected: 2 issues (both rule 1), differing only by sink subkind.
  void twoSinkSubkindsSameSource() {
    Object x = Origin.source();
    sinkA(x);
    sinkB(x);
  }

  // Case 9: Two sink subkinds at the same sink via multi-arg.
  // sinkAAndB has Argument(0) -> Sink(A), Argument(1) -> Sink(B).
  // The same Origin.source() flows to both arguments.
  // Expected: 1 issue (rule 1) with two sinks in the sinks array,
  // differing by sink port (Argument(0) vs Argument(1)) and subkind (A vs B).
  void twoSinkSubkindsSameSink() {
    Object x = Origin.source();
    sinkAAndB(x, x);
  }
}
