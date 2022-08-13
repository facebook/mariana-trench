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
  public static final Object staticFinalField = new Object();

  public static String staticStringField;
  public final String finalStringField = "final";
  public static final String staticFinalStringField = "Source";

  public Object noTaint;

  public void flow() {
    Origin.sink(field);
    Origin.sink(staticField);
    Origin.sink(staticFinalField);

    stringSink(staticStringField);
    // If a field is final and primitive, then we don't see issues from it
    // since the sget (field access) opcode is optimized by the java compiler
    // into const/const_string opcodes loading the value of the field.
    //
    // Note that a final field that is not primitive is not optimized in this way
    // so we still see an issue from staticFinalField above.
    stringSink(finalStringField);
    stringSink(staticFinalStringField);
  }

  public void stringSink(String argument) {
    return;
  }

  public Object get() {
    return field;
  }

  public void put(Object o) {
    field = o;
  }

  public void putObscure(Object o) {}
}
