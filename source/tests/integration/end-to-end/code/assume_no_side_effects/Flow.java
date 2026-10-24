/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

class DebugLog {
  // Matched by `-assumenosideeffects` in `assume_no_side_effects.pro`.
  public static void d(String tag, Object message) {}

  public static int v(String tag, Object message) {
    return 0;
  }

  // Not matched.
  public static void e(String tag, Object message) {}
}

public class Flow {
  static void strippedDebugLog() {
    DebugLog.d("tag", Origin.source());
  }

  static void keptErrorLog() {
    DebugLog.e("tag", Origin.source());
  }

  static void strippedUnusedResult() {
    DebugLog.v("tag", Origin.source());
  }

  static int keptUsedResult() {
    return DebugLog.v("tag", Origin.source());
  }

  static void debugLogWrapper(Object message) {
    DebugLog.d("tag", message);
  }

  static void strippedThroughWrapper() {
    debugLogWrapper(Origin.source());
  }
}
