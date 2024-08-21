/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

class ShimsWithArgumentGenerations {
  public static void sourceOnArgument0(Object source) {}

  public static void sourceOnArgument1(int i, Object source) {}

  public static void shimToSourceOnArgument0(Object source) {}

  // Tests with argument re-ordering.
  public static void shimToSourceOnArgument1(Object source, int i) {}

  public static void issue1() {
    Object source = null;
    shimToSourceOnArgument0(source);
    Origin.sink(source);
  }

  public static void issue2() {
    Object source = null;
    shimToSourceOnArgument1(source, 1);
    Origin.sink(source);
  }
}

class ShimsWithThisGenerations {
  private Object mField;

  public void sourceAndSinkOnThis() {
    // Argument(0).this has both a source and sink. However, this order or assignment should not
    // result in an issue.
    Origin.sink(mField);
    mField = Origin.source();
  }

  public void shimToSourceAndSinkOnThis() {}

  public void noIssueViaShim() {
    // Verify that processing the shim, which would taint Argument(0).mField with both source and
    // sink, does not result in the taint flowing back on itself, i.e. generations must be applied
    // *after* checking call flows (similar to analyzing a regular function).
    shimToSourceAndSinkOnThis();
  }

  public void noIssueWithoutShim() {
    // Control for `noIssueViaShim()`. We do not find an issue if the flow does not go via the
    // shim, we should not find it if it does too.
    sourceAndSinkOnThis();
  }
}
