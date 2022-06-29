/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class ArrayAllocation {
  /**
   * If there are rules that include "ArrayAllocation" as a sink (or strangely, a source), the
   * analysis would create a sink at the new operator. However, the sink should be absent if there
   * are no rules that use "ArrayAllocation".
   */
  public int[] allocateArray(int size) {
    return new int[size * 2];
  }
}
