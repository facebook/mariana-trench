/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  public Object field;

  // Test that we only infer generations at the end of the flow.

  public void add_generation() {
    this.field = Origin.source();
    this.field = new Object();
  }

  public void no_issue_generation() {
    add_generation();
    Origin.sink(this.field);
  }

  // TODO(T144485000): With a backward analysis, we now have a false positive here.

  public void add_propagation(Object x) {
    this.field = x;
    this.field = new Object();
  }

  public void issue_propagation() {
    add_propagation(Origin.source());
    Origin.sink(this.field);
  }

  // The following example requires inlining setters to avoid a false positive.

  public void field_setter(Object x) {
    this.field = x;
  }

  public Object field_getter() {
    return this.field;
  }

  public void no_issue_inlining() {
    field_setter(new Object());
    Origin.sink(field_getter());
  }
}
