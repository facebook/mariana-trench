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
}
