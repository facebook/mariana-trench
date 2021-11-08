/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class TestClass {
  public void flow(TaintedFieldClass argument) {
    // Field through function argument
    Origin.sink(argument.field);
    Origin.sink(argument.staticField);

    // Field through locally constructed object
    TaintedFieldClass flow = new TaintedFieldClass();
    Origin.sink(flow.field);

    Origin.sink(TaintedFieldClass.staticField);
  }
}
