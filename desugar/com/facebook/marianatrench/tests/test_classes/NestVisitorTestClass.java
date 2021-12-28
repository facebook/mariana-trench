/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
