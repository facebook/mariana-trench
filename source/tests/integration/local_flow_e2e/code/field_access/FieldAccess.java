/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class FieldAccess {
  public Object value;
  public static Object staticField;

  public Object getter() {
    return this.value;
  }

  public void setter(Object v) {
    this.value = v;
  }

  public static Object getStatic() {
    return staticField;
  }

  public static void setStatic(Object v) {
    staticField = v;
  }
}
