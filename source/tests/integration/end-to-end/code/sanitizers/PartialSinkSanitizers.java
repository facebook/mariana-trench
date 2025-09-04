/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class PartialSinkSanitizers {
  public static Object sourceA() {
    return new Object();
  }

  public static Object sourceB() {
    return new Object();
  }

  public static void multiSinks(Object a, Object b) {}

  public void triggeredPartialSinkForSourceA(Object object) {
    // Expect triggered PartialSink for partial label "arg0" to be
    // inferred for triggeredPartialSinkForSourceA()
    PartialSinkSanitizers.multiSinks(object, PartialSinkSanitizers.sourceB());
  }

  public void triggeredPartialSinkForSourceB(Object object) {
    // Expect triggered PartialSink for partial label "arg1" to be
    // inferred for triggeredPartialSinkForSourceB()
    PartialSinkSanitizers.multiSinks(PartialSinkSanitizers.sourceA(), object);
  }

  public void sanitizedPartialSinkForSourceA(Object object) {
    // Expect no triggered PartialSink to be inferred for sanitizedPartialSinkForSourceA().
    PartialSinkSanitizers.multiSinks(object, PartialSinkSanitizers.sourceB());
  }

  public void sanitizedPartialSinkForSourceB(Object object) {
    // Expect no triggered PartialSink to be inferred for sanitizedPartialSinkForSourceB().
    PartialSinkSanitizers.multiSinks(PartialSinkSanitizers.sourceA(), object);
  }

  public void testMultiSourceIssue1() {
    triggeredPartialSinkForSourceA(PartialSinkSanitizers.sourceA());
  }

  public void testMultiSourceIssue2() {
    triggeredPartialSinkForSourceB(PartialSinkSanitizers.sourceB());
  }

  public void testMultiSourceNoIssue1() {
    sanitizedPartialSinkForSourceA(PartialSinkSanitizers.sourceA());
  }

  public void testMultiSourceNoIssue2() {
    sanitizedPartialSinkForSourceB(PartialSinkSanitizers.sourceB());
  }
}
