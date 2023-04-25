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

public class IntentRouter extends Activity {
  // Expected: Issue to be emitted here.
  void routesIntent(Context context) {
    Intent intent = new Intent(context, IntentTarget.class);
    intent.putExtra(IntentTarget.SESSION, (String) Origin.source());

    // Shims to IntentTarget.readsRoutedIntent*() methods.
    startActivity(intent);
  }

  void doesNotRouteIntent_noStartActivity(Context context) {
    Intent intent = new Intent(context, IntentTarget.class);
    intent.putExtra(IntentTarget.SESSION, (String) Origin.source());
  }

  void doesNotRouteIntent_noIntentTarget() {
    Intent intent = new Intent();
    intent.putExtra(IntentTarget.SESSION, (String) Origin.source());
    startActivity(intent);
  }

  void doesNotRouteIntent_noTaintSources(Context context) {
    Intent intent = new Intent(context, IntentTarget.class);
    startActivity(intent);
  }

  // Needs fixed point analysis that tracks the flow of the intent argument
  // and its associated target classes into startActivity.
  void falseNegative_targetClassUnknown(Intent intent) {
    intent.putExtra(IntentTarget.SESSION, (String) Origin.source());
    startActivity(intent);
  }

  // Expected: Argument(1) -> Sink
  void objectTaintUnknown(String string, Context context) {
    Intent intent = new Intent(context, IntentTarget.class);
    intent.putExtra(IntentTarget.SESSION, string);
    startActivity(intent); // Has call edge to routing targets
  }

  // Expected: Issue to be emitted here.
  void issueViaIntentRouter(Context context) {
    this.objectTaintUnknown((String) Origin.source(), context);
  }
}
