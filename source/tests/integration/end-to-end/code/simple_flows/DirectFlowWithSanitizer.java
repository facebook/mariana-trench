/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class DirectFlowWithSanitizer {
  public void test() {
    Object x = Origin.source();
    Object y = Origin.sanitize(x);
    Origin.sink(y);
  }
}
