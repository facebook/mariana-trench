/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class ChildClass extends TaintedFieldClass {
  // Parent defines a sink on this field, but the shadowed
  // one in this class doesn't have a sink
  public Object shadowedField;

  public void flow() {
    // This field is defined in the parent with a sink
    field = Origin.source();
    shadowedField = Origin.source();
  }
}
