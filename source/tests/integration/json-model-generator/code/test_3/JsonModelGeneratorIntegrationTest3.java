/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class JsonModelGeneratorIntegrationTest3 {
  int function(int a, int b) {
    return a;
  }

  int functionB(
      @TestParameterAnnotation("Annotate on the parameter") int a,
      @TestParameterAnnotation("Annotate on the parameter") int b,
      @TestParameterAnnotation("Annotate on the parameter") int c) {
    return b;
  }

  static int functionC(
      @TestParameterAnnotation("Annotate on the parameter") int a,
      int b,
      @TestParameterAnnotation("Annotate on the parameter") int c) {
    return b;
  }
}
