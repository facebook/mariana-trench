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

  public Object getSourceB() {
    return new Object();
  }

  public Object getSanitizedSource() {
    return Origin.source();
  }

  public void toSink(Object argument) {
    Origin.sink(argument);
  }

  public void toSinkB(Object argument) {}

  public void toSanitizedSink(Object argument) {
    toSink(argument);
  }

  public void toSanitizedKindSink(Object argument) {
    toSink(argument);
    toSinkB(argument);
  }

  public Object[] getSanitizedKindSource() {
    Object[] array = new Object[2];
    array[0] = getSourceB();
    array[1] = getSource();
    return array;
  }

  public Object sanitizedPropagation(Object argument) {
    return argument;
  }

  public void toArraySink(Object[] array) {
    for (Object source : array) {
      toSink(source);
    }
  }

  public void sanitizedFlows() {
    toSanitizedSink(getSource());
    toSink(getSanitizedSource());

    // Kind-specific sanitizers
    toSanitizedKindSink(getSource());
    toArraySink(getSanitizedKindSource());

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
