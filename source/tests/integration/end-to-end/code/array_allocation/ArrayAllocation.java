/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class ArrayAllocation {
  public void integerArray() {
    int size = ArrayAllocation.source();
    int[] array = new int[size];
  }

  public void integerArrayTwo() {
    int size = (int) Origin.source();
    int[] array = new int[size];
  }

  public void integerArrayThree() {
    Integer size = (Integer) Origin.source();
    int[] array = new int[size];
  }

  public void objectArray() {
    int size = ArrayAllocation.source();
    Object[] array = new Object[size];
  }

  public void multidimentionalArray() {
    int size1 = 1;
    int size2 = ArrayAllocation.source();
    int[][] array = new int[size1][size2];
  }

  public void multidimentionalArrayTwo() {
    int size1 = ArrayAllocation.source();
    int size2 = 1;
    int[][] array = new int[size1][size2];
  }

  public void multidimentionalArrayThree() {
    int size1 = 1;
    int size2 = 1;
    int size3 = ArrayAllocation.source();
    int[][][] array = new int[size1][size2][size3];
  }

  public void multidimentionalObjectArray() {
    int size1 = 1;
    int size2 = ArrayAllocation.source();
    Object[][] array = new Object[size1][size2];
  }

  public void integerArrayInterProcedural() {
    int size = ArrayAllocation.source();
    allocateArray(size);
  }

  private void allocateArray(int size) {
    int[] array = new int[size * 2];
  }

  static int source() {
    return 0;
  }
}
