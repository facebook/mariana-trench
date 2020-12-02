// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

public class HasAnnotation {
  @TestMethodAnnotation("Annotate on the method declaration")
  void functionA() {}

  MyClass functionB() {
    return null;
  }
}

@TestClassAnnotation("Annotate on the class type")
class MyClass {}
