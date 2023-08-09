/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import java.util.Random;

class ExtraTraces {
  public static Object hopToSource1() {
    return hopToSource2();
  }

  public static Object hopToTransformT1(Object o) {
    return TaintTransforms.transformT1(o);
  }

  public static Object hopToSource2() {
    Object source = hopToSource3();

    int rand = new Random().nextInt(2);
    if (rand == 0) {
      // Expect extra_trace to hopTotransformT1().
      return hopToTransformT1(source);
    } else {
      // Expect extra_trace to transformT1().
      return TaintTransforms.transformT1(source);
    }
  }

  public static Object hopToSource3() {
    return TaintTransforms.getNewSource();
  }

  public static Object hopPropagation1(Object o) {
    return hopPropagation2(o);
  }

  public static Object hopPropagation2(Object o) {
    // Expect extra_trace to transformT2().
    Object withT2 = TaintTransforms.transformT2(o);
    return hopPropagation3(withT2);
  }

  public static Object hopPropagation3(Object o) {
    // Do not expect extra_trace to transformT2()
    TaintTransforms.transformT2(o);
    return hopPropagation4(o);
  }

  public static Object hopPropagation4(Object o) {
    int rand = new Random().nextInt(2);
    if (rand == 0) {
      return TaintTransforms.transformT1(o);
    }
    // Expect extra_trace to transformT2().
    return TaintTransforms.transformT2(o);
  }

  public static void hopToSink1(Object o) {
    hopToSink2(o);
  }

  public static void hopToSink2(Object o) {
    hopToSink3(o);
  }

  public static void hopToSink3(Object o) {
    int rand = new Random().nextInt(2);
    Object withTransform;
    if (rand == 0) {
      // Expect extra_trace to hopTotransformT1().
      withTransform = hopToTransformT1(o);
    } else {
      // Expect extra_trace to transformT2().
      withTransform = TaintTransforms.transformT2(o);
    }
    hopToSink4(withTransform);
  }

  public static void hopToSink4(Object o) {
    Origin.sink(o);
  }

  public static void testExtraTraces() {
    Object fromHopToSource = hopToSource1();
    // Expect extra_trace to transformT1().
    Object withT1 = TaintTransforms.transformT1(fromHopToSource);
    // expect extra_trace to hopPropagation1().
    Object withT1AndHopPropagation = hopPropagation1(withT1);
    hopToSink1(withT1AndHopPropagation);
  }
}
