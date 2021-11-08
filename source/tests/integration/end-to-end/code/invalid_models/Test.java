/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Test {
  public void transferTaint(String str, Object argument) {}

  public void flow() {
    Object source = Origin.source();
    String str = "";
    transferTaint(str, source);
    // Model for transferTaint() specifies an invalid parameter number and hence will not find an
    // issue here. The analysis however should not crash even with the invalid model.
    Origin.sink(str);
  }
}
