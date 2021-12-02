/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

class Constants {
  public static final int FOO = 42;
  public static final String BAR = "Bar";
}

public class ViaValueOf {
  Object getSourceViaInt(int arg1) {
    return new Object();
  }

  Object getSourceViaString(String arg1) {
    return new Object();
  }

  Object getSourceViaInvalidModel() {
    return new Object();
  }

  void sinkViaInt(int arg1, Object arg2) {}

  void sinkViaString(String arg1, Object arg2) {}

  void sinkViaInvalidModel(Object arg1) {}

  Object propagationReturn(Object obj) {
    return obj;
  }

  void someProcessing(Object obj) {}

  String propagationReturnString(String s) {
    return s;
  }

  String getString() {
    return "String";
  }

  void testSourceIntLiteral() {
    Object source = getSourceViaInt(42);
    Origin.sink(source);
  }

  void testSourceIntConstant() {
    Object source = getSourceViaInt(Constants.FOO);
    Origin.sink(source);
  }

  void testSourceStringLiteral() {
    Object source = getSourceViaString("Foo");
    Origin.sink(source);
  }

  void testSourceStringConstant() {
    Object source = getSourceViaString(Constants.BAR);
    Origin.sink(source);
  }

  void testSinkIntLiteral() {
    Object source = Origin.source();
    sinkViaInt(42, source);
  }

  void testSinkIntConstant() {
    Object source = Origin.source();
    sinkViaInt(Constants.FOO, source);
  }

  void testSinkStringLiteral() {
    Object source = Origin.source();
    sinkViaString("Foo", source);
  }

  void testSinkStringConstant() {
    Object source = Origin.source();
    sinkViaString(Constants.BAR, source);
  }

  void testHops() {
    Object source = getSourceViaString(Constants.BAR);
    someProcessing(source);
    sinkViaString("Foo", propagationReturn(source));
  }

  void testInvalidModel() {
    Object source = getSourceViaInvalidModel();
    sinkViaInvalidModel(source);
  }

  void testUnknown1() {
    String s1 = getString();
    Object source = getSourceViaString(s1);
    String s2 = "tito";
    String s22 = propagationReturnString(s2);
    sinkViaString(s22, source);
  }

  void testUnknown2() {
    String s1 = "s1";
    String s11 = (String) propagationReturn(s1);
    Object source = getSourceViaString(s11);
    String s2 = "s".concat("concat");
    sinkViaString(s2, source);
  }
}
