/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class MockObjectWithThisPropagation {
  private Object field;

  public MockObjectWithThisPropagation() {}

  public void setField(Object field) {
    // The "this"-propagation occurs here.
    this.field = field;
  }
}
