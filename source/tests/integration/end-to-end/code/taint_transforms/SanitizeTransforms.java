/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class SanitizeTransforms {

  static Object sourceA() {
    return new Object();
  }

  static void sinkB(Object o) {}

  static Object sanitizeSourceA(Object o) {
    return o;
  }

  static Object sanitizeSinkB(Object o) {
    return o;
  }

  static Object transformX(Object o) {
    return o;
  }

  static Object transformY(Object o) {
    return o;
  }

  static Object transformZ(Object o) {
    return o;
  }

  static Object taintWithTransformX() {
    Object o1 = sourceA();
    Object o2 = transformX(o1);
    return o2;
  }

  static Object sourceASanitizeA() {
    Object o1 = sourceA();
    Object o2 = sanitizeSourceA(o1);
    return o2;
  }

  static Object sourceASanitizeB() {
    Object o1 = sourceA();
    Object o2 = sanitizeSinkB(o1);
    return o2;
  }

  static Object taintWithTransformXY() {
    Object o1 = sourceA();
    Object o2 = transformX(o1);
    Object o3 = transformY(o2);
    return o3;
  }

  /* ... -> source sanitizer -> ... */

  static void testAToSanitizeAToBNoIssue() {
    Object o1 = sourceA();
    Object o2 = sanitizeSourceA(o1);
    sinkB(o2);
  }

  /* ... -> transforms -> source sanitizer -> ... */

  static void testAToXToSanitizeAtoBIssue() {
    Object o1 = sourceA();
    Object o2 = transformX(o1);
    Object o3 = sanitizeSourceA(o2);
    sinkB(o3);
  }

  static Object transformXSanitizeA(Object o) {
    Object o1 = transformX(o);
    Object o2 = sanitizeSourceA(o1);
    return o2;
  }

  static void testAToXSanitizeAToBIssue() {
    Object o1 = sourceA();
    Object o2 = transformXSanitizeA(o1);
    sinkB(o2);
  }

  static void sanitizeASinkB(Object o) {
    Object o1 = sanitizeSourceA(o);
    sinkB(o1);
  }

  static void testAToXToSanitizeABIssue() {
    Object o1 = sourceA();
    Object o2 = transformX(o1);
    sanitizeASinkB(o2);
  }

  static void testAXToSanitizeABIssue() {
    Object o1 = taintWithTransformX();
    sanitizeASinkB(o1);
  }

  static void transformXSanitizeASinkB(Object o) {
    Object o1 = transformX(o);
    Object o2 = sanitizeSourceA(o1);
    sinkB(o2);
  }

  static void testAToXSanitizeABIssue() {
    Object o1 = sourceA();
    transformXSanitizeASinkB(o1);
  }

  /* ... -> source sanitizer -> transforms -> ... */

  static void testAToSanitizeAtoXToBNoIssue() {
    Object o1 = sourceA();
    Object o2 = sanitizeSourceA(o1);
    Object o3 = transformX(o2);
    sinkB(o3);
  }

  static Object sanitizeATransformX(Object o) {
    Object o1 = sanitizeSourceA(o);
    Object o2 = transformX(o1);
    return o2;
  }

  static void testAToSanitizeAXToBNoIssue() {
    Object o1 = sourceA();
    Object o2 = sanitizeATransformX(o1);
    sinkB(o2);
  }

  static void transformXSinkB(Object o) {
    Object o1 = transformX(o);
    sinkB(o1);
  }

  static void testAToSanitizeAToXBNoIssue() {
    Object o1 = sourceA();
    Object o2 = sanitizeSourceA(o1);
    transformXSinkB(o2);
  }

  static void testASanitizeAToXBNoIssue() {
    Object o1 = sourceASanitizeA();
    transformXSinkB(o1);
  }

  static void sanitizeATransformXSinkB(Object o) {
    Object o1 = sanitizeSourceA(o);
    Object o2 = transformX(o1);
    sinkB(o2);
  }

  static void testAToSanitizeAXBNoIssue() {
    Object o1 = sourceA();
    sanitizeATransformXSinkB(o1);
  }

  static void sanitizeAToYToZToB(Object o) {
    Object o1 = sanitizeSourceA(o);
    Object o2 = transformY(o1);
    Object o3 = transformZ(o2);
    sinkB(o3);
  }

  static void testAToSanitizeAYZBNoIssue() {
    Object o1 = sourceA();
    sanitizeAToYToZToB(o1);
  }

  static void testAXYToSanitizeAYZBIssue() {
    Object o1 = taintWithTransformXY();
    sanitizeAToYToZToB(o1);
  }

  /* ... -> sink sanitizer -> ... */

  static void testAToSanitizeBToBNoIssue() {
    Object o1 = sourceA();
    Object o2 = sanitizeSinkB(o1);
    sinkB(o2);
  }

  /* ... -> transforms -> sink sanitizer -> ... */

  static void testAToXToSanitizeBToBNoIssue() {
    Object o1 = sourceA();
    Object o2 = transformX(o1);
    Object o3 = sanitizeSinkB(o2);
    sinkB(o3);
  }

  static Object transformXSanitizeB(Object o) {
    Object o1 = transformX(o);
    Object o2 = sanitizeSinkB(o1);
    return o2;
  }

  static void testAToXSanitizeBToBNoIssue() {
    Object o1 = sourceA();
    Object o2 = transformXSanitizeB(o1);
    sinkB(o2);
  }

  static Object sourceATransformXSanitizeB() {
    Object o1 = sourceA();
    Object o2 = transformX(o1);
    Object o3 = sanitizeSinkB(o2);
    return o3;
  }

  static void testAXSanitizeBToBNoIssue() {
    Object o1 = sourceATransformXSanitizeB();
    sinkB(o1);
  }

  /* ... -> sink sanitizer -> transforms -> ... */

  static void testAToSanitizeBToXToBIssue() {
    Object o1 = sourceA();
    Object o2 = sanitizeSinkB(o1);
    Object o3 = transformX(o2);
    sinkB(o3);
  }

  static Object sanitizeBTransformX(Object o) {
    Object o1 = sanitizeSinkB(o);
    Object o2 = transformX(o1);
    return o2;
  }

  static void testAToSanitizeBXToBIssue() {
    Object o1 = sourceA();
    Object o2 = sanitizeBTransformX(o1);
    sinkB(o2);
  }

  static void testASanitizeBToXToBIssue() {
    Object o1 = sourceASanitizeB();
    Object o2 = transformX(o1);
    sinkB(o2);
  }

  static void testASanitizeBToXBIssue() {
    Object o1 = sourceASanitizeB();
    transformXSinkB(o1);
  }

  static Object sourceASanitizeBTransformX() {
    Object o1 = sourceA();
    Object o2 = sanitizeSinkB(o1);
    Object o3 = transformX(o2);
    return o3;
  }

  static void testASanitizeBXToBIssue() {
    Object o1 = sourceASanitizeBTransformX();
    sinkB(o1);
  }

  /* ... -> Loop of sanitizers | transforms -> ... */

  static void testAToUnboundedLoopToB() {
    Object o1 = sourceA();

    while (true) {
      int n = new java.util.Random().nextInt(10);

      switch (n) {
        case 0:
          o1 = sanitizeSourceA(o1);
          continue;
        case 1:
          o1 = transformX(o1);
          continue;
        default:
          sinkB(o1);
          return;
      }
    }
  }

  /* ... -> sink sanitizer -> source sanitizer ->  ... */

  static void testAToSanitizeBToSanitizeAToBNoIssue() {
    Object o1 = sourceA();
    Object o2 = sanitizeSinkB(o1);
    Object o3 = sanitizeSourceA(o2);
    sinkB(o3);
  }

  /* ... -> sink sanitizer -> source sanitizer -> transforms ->  ... */

  static void testAToSanitizeBToSanitizeAToXToBNoIssue() {
    Object o1 = sourceA();
    Object o2 = sanitizeSinkB(o1);
    Object o3 = sanitizeSourceA(o2);
    Object o4 = transformX(o3);
    sinkB(o4);
  }

  /* ... -> transforms -> sink sanitizer -> source sanitizer ->  ... */

  static void testAToXToSanitizeBToSanitizeAToBNoIssue() {
    Object o1 = sourceA();
    Object o2 = transformX(o1);
    Object o3 = sanitizeSinkB(o2);
    Object o4 = sanitizeSourceA(o3);
    sinkB(o4);
  }

  static void testAToXSanitizeBToSanitizeAToBNoIssue() {
    Object o1 = sourceA();
    Object o2 = transformXSanitizeB(o1);
    Object o3 = sanitizeSourceA(o2);
    sinkB(o3);
  }

  static Object sanitizeBSanitizeB(Object o) {
    Object o1 = sanitizeSinkB(o);
    Object o2 = sanitizeSinkB(o1);
    return o2;
  }

  static void testAToSanitizeBSanitizeBToSanitizeAToXToBNoIssue() {
    Object o1 = sourceA();
    Object o2 = sanitizeBSanitizeB(o1);
    Object o3 = sanitizeSourceA(o2);
    Object o4 = transformX(o3);
    sinkB(o4);
  }

  /* sanitizer method that also has other transforms */

  // TODO: T189558338 Figure out the desirable order of sanitizers
  // currently the sanitizers are always added to the front of transformlist
  // so the model would be sanitize[A] -> transform[X] -> localreturn
  static Object transformXwithSanitizeA(Object o) {
    Object o1 = transformX(o);
    return o1;
  }

  // Should have same model as transformXwithSanitizeA
  static Object transformXSanitizeAwithSanitizeA(Object o) {
    Object o1 = sanitizeSourceA(o);
    Object o2 = transformX(o1);
    return o2;
  }

  static void testATotransformXwithSanitizeAToBNoIssue() {
    Object o1 = sourceA();
    Object o2 = transformXwithSanitizeA(o1);
    sinkB(o2);
  }

  static void testATotransformXSanitizeAwithSanitizeAToBNoIssue() {
    Object o1 = sourceA();
    Object o2 = transformXSanitizeAwithSanitizeA(o1);
    sinkB(o2);
  }

  /* Test ordering in UI traces */

  static Object taintWithTransformY() {
    Object o1 = sourceA();
    Object o2 = transformY(o1);
    return o2;
  }

  static Object taintWithYSanitizeB() {
    Object o1 = taintWithTransformY();
    Object o2 = sanitizeSinkB(o1);
    return o2;
  }

  static Object taintWithYSanitizeBSanitizeA() {
    Object o1 = taintWithYSanitizeB();
    Object o2 = sanitizeSourceA(o1);
    return o2;
  }

  static Object taintWithYSanitizeBSanitizeAToZ() {
    Object o1 = taintWithYSanitizeBSanitizeA();
    Object o2 = transformZ(o1);
    return o2;
  }

  static void testAToYToSantizeBToSanitizeAToZToBIssue() {
    Object o1 = taintWithYSanitizeBSanitizeAToZ();
    sinkB(o1);
  }

  static Object taintWithYSanitizeA() {
    Object o1 = taintWithTransformY();
    Object o2 = sanitizeSourceA(o1);
    return o2;
  }

  static Object taintWithYSanitizeASanitizeB() {
    Object o1 = taintWithYSanitizeA();
    Object o2 = sanitizeSinkB(o1);
    return o2;
  }

  static Object taintWithYSanitizeASanitizeBToZ() {
    Object o1 = taintWithYSanitizeASanitizeB();
    Object o2 = transformZ(o1);
    return o2;
  }

  static void testAToYToSantizeAToSanitizeBToZToBIssue() {
    Object o1 = taintWithYSanitizeASanitizeBToZ();
    sinkB(o1);
  }
}
