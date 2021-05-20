/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class GlobalSanitizers {

  public Object getSource() {
    return Origin.source();
  }

  public Object getSanitizedSource() {
    return Origin.source();
  }

  public void toSink(Object argument) {
    Origin.sink(argument);
  }

  public void toSanitizedSink(Object argument) {
    toSink(argument);
  }

  public Object sanitizedPropagation(Object argument) {
    return argument;
  }

  public void sanitizedFlows() {
    toSanitizedSink(getSource());
    toSink(getSanitizedSource());

    toSink(sanitizedPropagation(getSource()));

    // User-specified sink is not propagated
    sanitizedSinkWithReturn(getSource());
  }

  public Object sanitizedSinkWithReturn(Object argument) {
    toSink(argument);

    toSink(getSource());
    // User-specified return sink is not cleared by sanitizer on this method
    return argument;
  }
}
