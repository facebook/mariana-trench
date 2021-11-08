/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class TaintedFieldClass {
  public Object field;
  public static Object staticField;

  public void flow() {
    Origin.sink(field);
    Origin.sink(staticField);
  }
}
