/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

public class IntentRouterAndTarget extends Activity {
  static final String SESSION2 = "session2";

  // A method can re-route Intent data into another Activity.
  // Expected: Intent."session2" -> Sink
  void routesAndGetsIntent(Context context) {
    Intent intent = this.getIntent();
    String tainted = intent.getStringExtra(SESSION2);

    intent = new Intent(context, IntentTarget.class);
    intent.putExtra(IntentTarget.SESSION, tainted);

    // Shims to IntentTarget.readsRoutedIntent*() methods.
    startActivity(intent);
  }
}
