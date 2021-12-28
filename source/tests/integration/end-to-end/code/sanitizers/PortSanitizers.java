/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class PortSanitizers {

  Object mField;

  public void sanitizedSinkPortAndKind(Object argument1, Object sanitizesSinks) {
    Origin.sink(argument1);
    Origin.sink(sanitizesSinks);
    GlobalSanitizers.toSinkB(sanitizesSinks);
  }

  public Object sanitizedSourcePort() {
    mField = Origin.source();
    return Origin.source();
  }

  public Object[] sanitizedPropagationPort(Object argument1, Object argument2) {
    Object[] array = new Object[2];
    array[0] = argument1;
    array[1] = argument2;
    return array;
  }

  public void sanitizedFlows() {
    sanitizedSinkPortAndKind(Origin.source(), Origin.source());

    Origin.sink(sanitizedSourcePort());
    Origin.sink(this);

    sanitizedPropagationPort(Origin.source(), Origin.source());
  }
}
