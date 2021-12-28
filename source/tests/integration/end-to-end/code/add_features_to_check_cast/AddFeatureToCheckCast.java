/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class AddFeatureToCheckCast {
  void sink(String s) {}

  void testString() {
    Object source = Origin.source();
    String source_str = (String) source;
    sink(source_str);
  }

  void testStringNoViaCast() {
    Object source = Origin.source();
    String source_str = (String) source; // Should not attach feature to the original location.
    Origin.sink(source);
  }

  public void testWithViaNumerical() {
    Integer size = (Integer) Origin.source();
    int[] array = new int[size * 2];
  }

  public void testInterProcedural() {
    String source_str = (String) Origin.source();
    toSink(source_str);
  }

  public void testInterproceduralWithViaNumerical() {
    Integer size = (Integer) Origin.source();
    arrayAllocate(size);
  }

  private void toSink(String str) {
    sink(str);
  }

  public void arrayAllocate(int size) {
    int[] array = new int[size * 2];
  }
}
