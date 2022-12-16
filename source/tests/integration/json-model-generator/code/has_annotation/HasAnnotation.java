/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class HasAnnotation {
  @TestMethodAnnotation("Annotate on the method declaration")
  Object functionA() {
    return null;
  }

  MyClass functionB() {
    return null;
  }

  Object functionC(MyClass arg1) {
    return null;
  }
}

@TestClassAnnotation("Annotate on the class type")
class MyClass {
  Object functionD() {
    return null;
  }
}
