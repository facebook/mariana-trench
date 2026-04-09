/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Loops {
  public Object next;
  public Object value;

  public static Object whileLoop(Object initial, Object replacement) {
    Object result = initial;
    int i = 0;
    while (i < 10) {
      result = replacement;
      i++;
    }
    return result;
  }

  public static Object forEachArray(Object[] items) {
    Object last = null;
    for (Object item : items) {
      last = item;
    }
    return last;
  }

  public static Object linkedListTraversal(Loops head) {
    Loops current = head;
    while (current != null) {
      current = (Loops) current.next;
    }
    return current;
  }
}
