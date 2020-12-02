// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

public class Flow {
  public Object field;

  // Test that we only infer generations at the end of the flow.

  public void add_generation() {
    this.field = Origin.source();
    this.field = new Object();
  }

  public void issue_generation() {
    add_generation();
    Origin.sink(this.field);
  }

  // Test that we only infer propagations at the end of the flow.

  public void add_propagation(Object x) {
    this.field = x;
    this.field = new Object();
  }

  public void issue_propagation() {
    add_propagation(Origin.source());
    Origin.sink(this.field);
  }
}
