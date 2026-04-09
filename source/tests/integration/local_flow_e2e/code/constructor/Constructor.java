/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Constructor {
  public Object a;
  public Object b;

  public Constructor(Object a, Object b) {
    this.a = a;
    this.b = b;
  }

  public static Constructor create(Object x, Object y) {
    return new Constructor(x, y);
  }
}
