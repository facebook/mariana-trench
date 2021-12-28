/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Test {
  public void transferTaint(String str, Object argument) {}

  public Object sourceViaType() {
    return new Object();
  }

  public void sinkNonExistent(Object argument) {}

  public void testInvalidParameterPosition() {
    Object source = Origin.source();
    String str = "";
    transferTaint(str, source);
    // Model for transferTaint() specifies an invalid parameter number and hence will not find an
    // issue here. The analysis however should not crash even with the invalid model.
    Origin.sink(str);
  }

  public void testInvalidViaTypeOfSource() {
    // Model for sourceViaType() specifies an invalid parameter number as via_type_of feature.
    // However, we still generate the model and the analysis reports the issue.
    Origin.sink(sourceViaType());
  }

  public void testInvalidSink() {
    // Model for sinkNonExistent() specifies an invalid parameter number as the sink and hence will
    // not find an issue here.
    sinkNonExistent(Origin.source());
  }
}
