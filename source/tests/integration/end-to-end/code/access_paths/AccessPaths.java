/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import java.util.Random;

class Dictionary {
  Object field;

  // Dictionary like stubs
  void setIndex(String key, Object value) {}

  Dictionary putExtra(String key, Object value) {
    return this;
  }

  Object getIndex(String key) {
    return new Object();
  }

  void sinkIndex(String key) {}
}

class Constants {
  public static final String FOO = "foo";
}

public class AccessPaths {
  public static void testSimple() {
    Dictionary d = new Dictionary();
    d.field = new Object();
    d.setIndex("safe_field", new Object());
    d.setIndex("field", Origin.source());

    Origin.sink(d.field); // expect no issue
    Origin.sink(d.getIndex("safe_field")); // expect no issue
    Origin.sink(d.getIndex("field")); // expect issue
  }

  private static String getString() {
    return "string";
  }

  private static Object getDifferentSource() {
    return new Object();
  }

  private static Object indexAccessToSource(Dictionary d) {
    return d.getIndex("source");
  }

  private static void indexAccessToSink(Dictionary d) {
    Origin.sink(d.getIndex("source"));
  }

  private static void sinkIndex(Dictionary d, String index) {}

  private static void sinkIndexWrapper(Dictionary d) {
    d.sinkIndex("source");
  }

  public static void testStrongUpdate() {
    Dictionary d = new Dictionary();

    d.setIndex(Constants.FOO, Origin.source());
    d.setIndex(Constants.FOO, getDifferentSource());

    Origin.sink(d.getIndex(Constants.FOO)); // expect issue for DifferentSource only
  }

  public static void testWeakUpdate() {
    Dictionary d = new Dictionary();
    Random rand = new Random();

    if (rand.nextInt(2) == 0) {
      d.setIndex(Constants.FOO, Origin.source());
    } else {
      d.setIndex(Constants.FOO, getDifferentSource());
    }

    Origin.sink(d.getIndex(Constants.FOO)); // expect both issues
  }

  public static void testUnresolvedKey() {
    Dictionary d = new Dictionary();

    d.setIndex(getString(), Origin.source());
    d.setIndex(Constants.FOO, getDifferentSource());

    Origin.sink(d.getIndex(Constants.FOO)); // expect issue for DifferentSource only
    Origin.sink(d.getIndex("unknown")); // expect issues for Source only
    Origin.sink(d.getIndex(getString())); // expect issues for both sources
  }

  public static void testWrappersToSource() {
    Dictionary issue =
        new Dictionary().putExtra("safe", new Object()).putExtra("source", Origin.source());
    Origin.sink(indexAccessToSource(issue)); // expect issue for Source

    Dictionary safe = new Dictionary().putExtra("unused_source", getDifferentSource());
    Origin.sink(indexAccessToSource(safe)); // expect no issue
  }

  public static void testWrappersToSink() {
    Dictionary issue =
        new Dictionary().putExtra("safe", new Object()).putExtra("source", Origin.source());
    indexAccessToSink(issue); // expect issue

    Dictionary safe = new Dictionary().putExtra("safe", getDifferentSource());
    indexAccessToSink(safe); // expect no issue
  }

  public static void testWrappers() {
    Dictionary d = new Dictionary().putExtra("source", Origin.source());

    Dictionary issueWithSourceOnly =
        new Dictionary()
            .putExtra("unused_source", getDifferentSource())
            .putExtra("source", indexAccessToSource(d));
    indexAccessToSink(issueWithSourceOnly); // expect issue for Source only
  }

  public static void testSinkOnIndexFromValueOf() {
    Dictionary d =
        new Dictionary()
            .putExtra("source", Origin.source())
            .putExtra("differentSource", getDifferentSource());

    sinkIndex(d, "safe"); // expect no issue
    sinkIndex(d, "source"); // expect issue for Source only
    sinkIndex(d, "differentSource"); // expect issue for DifferentSource only
    sinkIndex(d, getString()); // expect issue for both Source and DifferentSource
  }

  public static void testSinkOnIndexFromValueOfWrapper() {
    Dictionary d = new Dictionary().putExtra("foo", Origin.source());

    sinkIndexWrapper(d); // expect no issue

    d.putExtra("source", getDifferentSource());
    sinkIndexWrapper(d); // expect issue for DifferentSource only
  }

  public static void testNoIssueStrongUpdateEmptyTaint() {
    Dictionary d = new Dictionary();

    d.setIndex(Constants.FOO, Origin.source());
    d.setIndex(Constants.FOO, "");

    Origin.sink(d.getIndex(Constants.FOO)); // expect no issue
  }

  public static void testNoSinkStrongUpdateEmptyTaint(String x) {
    Dictionary d = new Dictionary();

    d.setIndex(Constants.FOO, x);
    d.setIndex(Constants.FOO, "");

    Origin.sink(d.getIndex(Constants.FOO)); // expect no sink, currently a false positive.
  }
}
