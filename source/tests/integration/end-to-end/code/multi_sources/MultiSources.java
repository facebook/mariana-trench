/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class MultiSources {

  public static Object sourceA() {
    return new Object();
  }

  public static Object sourceB() {
    return new Object();
  }

  public static Object sourceD() {
    return new Object();
  }

  public static void sink(Object a, Object b) {}

  public void issue() {
    MultiSources.sink(MultiSources.sourceA(), MultiSources.sourceB());
  }

  public void issueViaTriggeredSinkB() {
    triggeredSinkB(MultiSources.sourceB());
  }

  public void triggeredSinkB(Object object) {
    // Sink "a" is triggered internally, but it is the triggered sink "b" that
    // gets propagated, hence the method name.
    MultiSources.sink(MultiSources.sourceA(), object);
  }

  public void issueViaTriggeredSinkA() {
    triggeredSinkA(MultiSources.sourceA());
  }

  public void triggeredSinkA(Object object) {
    Object source = null;
    if (object == null) {
      source = MultiSources.sourceB();
    } else {
      source = MultiSources.sourceD();
    }
    MultiSources.sink(object, source);
  }

  public void noSinks() {
    // Sinks are triggered internally, but should not be propagated.
    Object object = new Object();
    MultiSources.sink(object, MultiSources.sourceB());
    MultiSources.sink(MultiSources.sourceA(), object);
  }

  public void falseNegativeWithWrappers(Object a, Object b) {
    // This should in theory be propagated. However, it is currently handled
    // the same way as unfulfillablePartialSinkDoNotPropagate below: If the
    // analysis does not see a triggered sink, it drops the partial sink. To
    // make this work, the analysis must avoid dropping partial sinks if both
    // counterparts are propagated.
    MultiSources.sink(a, b);
  }

  public void unfulfillablePartialSinkDoNotPropagate(Object b) {
    // "b" is a partial sink, but its counterpart will never see a source.
    // There is never any point in propagating this partial sink.
    MultiSources.sink(new Object(), b);
  }
}
