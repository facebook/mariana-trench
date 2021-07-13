/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class FlowWithHops {

  public Object getSource() {
    return Origin.source();
  }

  public Object getSourceWithHop() {
    return this.getSource();
  }

  public void toSink(Object argument) {
    Origin.sink(argument);
  }

  public void toSinkWithHop(Object argument) {
    this.toSink(argument);
  }

  public void issue() {
    this.toSinkWithHop(this.getSourceWithHop());
  }
}
