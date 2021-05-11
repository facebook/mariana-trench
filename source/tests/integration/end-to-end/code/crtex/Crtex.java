/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import androidx.fragment.app.FragmentActivity;

class TestUri extends Uri {}

public class Crtex extends FragmentActivity {
  /* This is a typical crtex sink with a single canonical_name defined. */
  public static void startActivityHelper(Intent intent, Activity activity) {
    /* Usually something that calls activity.startActivity() */
  }

  public void producer_flow() {
    Intent intent = new Intent("test", new TestUri());
    Crtex.startActivityHelper(intent, this);
  }

  public void crtex_precondition_leaf(Intent intent) {
    Crtex.startActivityHelper(intent, this);
  }

  public void precondition(Intent intent) {
    crtex_precondition_leaf(intent);
  }

  /**
   * A crtex sink with more than one canonical names on one port, but a single canonical name on
   * another, and an invalid canonical name on the last.
   */
  public static void multiple_canonical_names(Intent intent, Activity activity, int i) {}

  /* The canonical names are instantiated in this method. */
  public static void multiple_canonical_names_precondition(
      Intent intent, Activity activity, int i) {
    Crtex.multiple_canonical_names(intent, activity, i);
  }

  /* Verifies that crtex sinks are propagated differently from regular sinks. */
  public static void crtex_with_regular_sink(Intent intent) {}

  public static void crtex_with_regular_sink_precondition(Intent intent) {
    Crtex.crtex_with_regular_sink(intent);
  }
}
