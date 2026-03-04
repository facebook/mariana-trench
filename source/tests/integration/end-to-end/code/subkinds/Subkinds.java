/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Subkinds {

  // Helper methods for sources with subkinds.
  // Models declare these as returning SourceKind(A), SourceKind(B), etc.
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
  // Models declare Argument(0) as SinkKind(A) and Argument(1) as SinkKind(B).
  static void sinkAAndB(Object a, Object b) {}

  // Helper for transforms.
  static Object transform(Object o) {
    return o;
  }

  // Case 1: Baseline — plain SourceKind -> SinkKind (no subkinds).
  // Expected: 1 issue (rule 1).
  void testBaselineFlow() {
    Object x = Origin.source();
    Origin.sink(x);
  }

  // Case 2: SourceKind with subkind -> SinkKind (no subkind).
  // SourceKind(A) matches rule 1 via discard_subkind().
  // Expected: 1 issue (rule 1).
  void testSourceSubkindFlow() {
    Object x = getSourceA();
    Origin.sink(x);
  }

  // Case 3: SourceKind (no subkind) -> SinkKind with subkind.
  // SinkKind(A) matches rule 1 via discard_subkind().
  // Expected: 1 issue (rule 1).
  void testSinkSubkindFlow() {
    Object x = Origin.source();
    sinkA(x);
  }

  // Case 4: SourceKind with subkind -> SinkKind with subkind.
  // SourceKind(A) -> SinkKind(A), both match rule 1 via discard_subkind().
  // Expected: 1 issue (rule 1).
  void testBothSubkindFlow() {
    Object x = getSourceA();
    sinkA(x);
  }

  // Case 5: SourceKind with subkind -> Transform -> SinkKind.
  // SourceKind(A) -> T1 -> SinkKind matches rule 2 (has transform T1).
  // Expected: 1 issue (rule 2).
  void testSourceSubkindWithTransformFlow() {
    Object x = getSourceA();
    x = transform(x);
    Origin.sink(x);
  }

  // Case 6: SourceKind -> Transform -> SinkKind with subkind.
  // SourceKind -> T1 -> SinkKind(A) matches rule 2 (has transform T1).
  // Expected: 1 issue (rule 2).
  void testSinkSubkindWithTransformFlow() {
    Object x = Origin.source();
    x = transform(x);
    sinkA(x);
  }

  // Case 7: Two SourceKind subkinds at the same sink.
  // SourceKind(A) and SourceKind(B) both flow to the same Origin.sink() call.
  // Expected: 1 issue (rule 1) with 2 source entries (SourceKind(A) and SourceKind(B)).
  void testTwoSourceSubkindsSameSink(boolean condition) {
    Object x;
    if (condition) {
      x = getSourceA();
    } else {
      x = getSourceB();
    }
    Origin.sink(x);
  }

  // Case 8: Two SinkKind subkinds at the same source.
  // The same Origin.source() flows to both sinkA() and sinkB().
  // Expected: 2 issues (both rule 1), differing only by sink subkind.
  void testTwoSinkSubkindsSameSource() {
    Object x = Origin.source();
    sinkA(x);
    sinkB(x);
  }

  // Case 9: Two SinkKind subkinds at the same sink via multi-arg.
  // sinkAAndB has Argument(0) -> SinkKind(A), Argument(1) -> SinkKind(B).
  // The same Origin.source() flows to both arguments.
  // Expected: 1 issue (rule 1) with two sinks in the sinks array,
  // differing by sink port (Argument(0) vs Argument(1)) and subkind (A vs B).
  void testTwoSinkSubkindsSameSink() {
    Object x = Origin.source();
    sinkAAndB(x, x);
  }

  // --- Sanitizer + subkind interaction tests ---

  // Helper that internally generates SourceKind(A) and returns it.
  // Has a source sanitizer for base kind "SourceKind" — should block SourceKind(A).
  static Object getSanitizedSourceBaseKind() {
    return getSourceA();
  }

  // Helper that internally generates SourceKind(A) and returns it.
  // Has a source sanitizer for subkind SourceKind(A) only.
  static Object getSanitizedSourceSubkindA() {
    return getSourceA();
  }

  // Returns either SourceKind(A) or SourceKind(B) depending on runtime condition.
  // Has a source sanitizer for subkind SourceKind(A) only — only SourceKind(B)
  // should propagate through as a generation.
  static Object getSanitizedSourceSubkindAOnly(Object condition) {
    if (condition != null) {
      return getSourceA();
    }
    return getSourceB();
  }

  // Helper that forwards to sinkA.
  // Has a sink sanitizer for base kind "SinkKind" — should block SinkKind(A).
  static void toSanitizedSinkBaseKind(Object o) {
    sinkA(o);
  }

  // Helper that forwards to sinkA and sinkB.
  // Has a sink sanitizer for subkind SinkKind(A) only.
  static void toSanitizedSinkSubkindA(Object o) {
    sinkA(o);
    sinkB(o);
  }

  // Propagation helpers.
  static Object propagateSanitizeSink(Object o) {
    return o;
  }

  static Object propagateSanitizeSinkSubkindA(Object o) {
    return o;
  }

  // Wrappers around propagation sanitizer helpers — no models needed, MT infers propagation.
  static Object wrapPropagateSanitizeSink(Object o) {
    return propagateSanitizeSink(o);
  }

  static Object wrapPropagateSanitizeSinkSubkindA(Object o) {
    return propagateSanitizeSinkSubkindA(o);
  }

  // Helpers that wrap propagation + sink into a single call.
  static void propagateAndSinkA(Object o) {
    o = wrapPropagateSanitizeSink(o);
    sinkA(o);
  }

  static void propagateSubkindAAndSinkAB(Object o) {
    o = wrapPropagateSanitizeSinkSubkindA(o);
    sinkA(o);
    sinkB(o);
  }

  // Case 10: Source sanitizer for base kind "SourceKind" should block SourceKind(A).
  // getSanitizedSourceBaseKind has: {"sanitize": "sources", "kinds": [{"kind": "SourceKind"}]}.
  // Expected: 0 issues.
  void testSourceSanitizerBaseKindBlocksSubkind() {
    Object x = getSanitizedSourceBaseKind();
    Origin.sink(x);
  }

  // Case 11: Source sanitizer for subkind "A" should block SourceKind(A).
  // getSanitizedSourceSubkindA has:
  //   {"sanitize": "sources", "kinds": [{"kind": "SourceKind", "subkind": "A"}]}.
  // Expected: 0 issues.
  void testSourceSanitizerSubkindBlocksExact() {
    Object x = getSanitizedSourceSubkindA();
    Origin.sink(x);
  }

  // Case 12: Source sanitizer for subkind "A" should block only SourceKind(A), not SourceKind(B).
  // getSanitizedSourceSubkindAOnly internally generates both SourceKind(A) and SourceKind(B)
  // but sanitizer only covers SourceKind(A).
  // Expected: 1 issue from SourceKind(B), 0 from SourceKind(A).
  void testSourceSanitizerSubkindBlocksOnlyMatching() {
    Object x = getSanitizedSourceSubkindAOnly(null);
    Origin.sink(x);
  }

  // Case 13: Sink sanitizer for base kind "SinkKind" should block SinkKind(A).
  // toSanitizedSinkBaseKind has: {"sanitize": "sinks", "kinds": [{"kind": "SinkKind"}]}.
  // Expected: 0 issues.
  void testSinkSanitizerBaseKindBlocksSubkind() {
    Object x = Origin.source();
    toSanitizedSinkBaseKind(x);
  }

  // Case 14: Sink sanitizer for subkind "A" should block SinkKind(A) but not SinkKind(B).
  // toSanitizedSinkSubkindA has: {"sanitize": "sinks", "kinds": [{"kind": "SinkKind", "subkind":
  // "A"}]}.
  // Expected: 1 issue for SinkKind(B), 0 for SinkKind(A).
  void testSinkSanitizerSubkindBlocksOnlyMatching() {
    Object x = Origin.source();
    toSanitizedSinkSubkindA(x);
  }

  // Case 15: Propagation sanitizer for base "SinkKind" should block SinkKind(A) propagation.
  // propagateSanitizeSink has propagation sanitizer: {"sanitize": "propagations",
  //   "port": "Argument(0)", "kinds": [{"kind": "Sink[SinkKind]"}]}.
  // Expected: 0 issues (SinkKind(A) is sanitized because SinkKind covers SinkKind(A)).
  void testPropagationSanitizerBaseKindBlocksSubkind() {
    Object x = Origin.source();
    x = propagateSanitizeSink(x);
    sinkA(x);
  }

  // Case 16: Propagation sanitizer for "SinkKind(A)" should block only SinkKind(A), not
  // SinkKind(B).
  // propagateSanitizeSinkSubkindA has propagation sanitizer: {"sanitize": "propagations",
  //   "port": "Argument(0)", "kinds": [{"kind": "Sink[SinkKind]", "subkind": "A"}]}.
  // Expected: 1 issue for SinkKind(B), 0 for SinkKind(A).
  void testPropagationSanitizerSubkindBlocksOnlyMatching() {
    Object x = Origin.source();
    x = propagateSanitizeSinkSubkindA(x);
    sinkA(x);
    sinkB(x);
  }

  // Case 17: Propagation sanitizer for base SinkKind should propagate through a wrapper.
  // wrapPropagateSanitizeSink calls propagateSanitizeSink (which has a propagation
  // sanitizer for Sink[SinkKind]). MT infers propagation for the wrapper, and the
  // sanitizer should carry through.
  // Expected: 0 issues.
  void testPropagationSanitizerThroughWrapper() {
    Object x = Origin.source();
    x = wrapPropagateSanitizeSink(x);
    sinkA(x);
  }

  // Case 18: Propagation sanitizer for SinkKind(A) should propagate through a wrapper,
  // blocking only SinkKind(A), not SinkKind(B).
  // wrapPropagateSanitizeSinkSubkindA calls propagateSanitizeSinkSubkindA (which has a
  // propagation sanitizer for Sink[SinkKind] with subkind A).
  // Expected: 1 issue for SinkKind(B), 0 for SinkKind(A).
  void testPropagationSanitizerSubkindThroughWrapper() {
    Object x = Origin.source();
    x = wrapPropagateSanitizeSinkSubkindA(x);
    sinkA(x);
    sinkB(x);
  }

  // Case 19: Propagation sanitizer through two layers of wrapping (wrapper + sink helper).
  // propagateAndSinkA calls wrapPropagateSanitizeSink then sinkA.
  // Expected: 0 issues.
  void testPropagationSanitizerThroughDoubleWrapper() {
    Object x = Origin.source();
    propagateAndSinkA(x);
  }

  // Case 20: Propagation sanitizer for SinkKind(A) through two layers of wrapping.
  // propagateSubkindAAndSinkAB calls wrapPropagateSanitizeSinkSubkindA then sinkA + sinkB.
  // Expected: 1 issue for SinkKind(B), 0 for SinkKind(A).
  void testPropagationSanitizerSubkindThroughDoubleWrapper() {
    Object x = Origin.source();
    propagateSubkindAAndSinkAB(x);
  }
}
