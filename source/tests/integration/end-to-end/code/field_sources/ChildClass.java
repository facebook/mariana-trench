/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class ChildClass extends TaintedFieldClass {
  public Object shadowedField;

  public void flow() {
    TaintedFieldClass test = new ChildClass();
    // No issue is expected here, java does not dynamically resolve fields
    Origin.sink(test.shadowedField);

    Origin.sink(shadowedField);
    // Field defined in parent class should still carry taint
    Origin.sink(field);
  }
}
