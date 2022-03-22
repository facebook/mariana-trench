/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class TaintedFieldClass {
  public Object field;
  public Object shadowedField;
  public static Object staticField;

  public static String staticStringField;
  public final String finalStringField = source();
  public static final String staticFinalStringField = staticSource();

  public void flow() {
    field = Origin.source();
    staticField = Origin.source();

    staticStringField = source();
  }

  public String source() {
    return "some sensitive string";
  }

  public static String staticSource() {
    return "sensitive string";
  }
}
