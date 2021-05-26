/*
 * Copyright (c) Facebook, Inc. and its affiliates.
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

  public void sanitizedFlows() {
    sanitizedSinkPortAndKind(Origin.source(), Origin.source());

    Origin.sink(sanitizedSourcePort());
    Origin.sink(this);
  }
}
