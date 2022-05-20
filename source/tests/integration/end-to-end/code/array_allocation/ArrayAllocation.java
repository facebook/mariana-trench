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

  public void integerArrayThroughPropagation() {
    int size1 = (int) Origin.source();
    int[] array1 = new int[size1];

    Integer size2 = (Integer) Origin.source();
    if (size1 > 0) {
      int[] array2 = new int[size2];
    } else {
      int[][] array3 = new int[size2][5];
    }
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
