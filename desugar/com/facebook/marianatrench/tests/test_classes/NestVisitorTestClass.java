// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench;

// This class is the input of unit tests
public class NestVisitorTestClass {
  int a;

  NestVisitorTestClass() {
    InnerClass innerClass = new InnerClass();
    int c = innerClass.b;
  }

  class InnerClass {
    int b;

    InnerClass() {
      b = a;
    }
  }
}
