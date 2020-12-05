/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
