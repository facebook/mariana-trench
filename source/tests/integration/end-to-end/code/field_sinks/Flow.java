/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {

  public void flow() {
    flow_to_field(Origin.source(), new Object());
    // No issue for this call
    flow_to_field(new Object(), Origin.source());
  }

  public void flow_to_field(Object argument, Object unused_argument) {
    TaintedFieldClass item = new TaintedFieldClass();
    item.field = argument;
  }
}
