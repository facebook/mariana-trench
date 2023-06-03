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

  // Intent target where none of its methods lead to a sink, or even calls
  // getIntent(). Taint cannot flow through this.
  class IntentTargetNoSink {}

  // Expected: No issues found.
  // However, IntentRoutingAnalyzer currently maintains the map:
  //   method -> [TargetIntents]
  // It does not track which startActivity() call flows into which target
  // Intent. As a result, the first startActivity() call is assumed to flow
  // into both IntentTargetNoSink and IntentTarget[WithSink]. The latter
  // triggers a false positive.
  void falsePositive_wrongShim(Context context) {
    Intent intent = new Intent(context, IntentTargetNoSink.class);
    intent.putExtra(IntentTarget.SESSION, (String) Origin.source());
    // Tainted intent does not reach any sink. No issues expected
    startActivity(intent);

    // intent2 can reach sinks, but is not tainted.
    Intent intent2 = new Intent(context, IntentTarget.class);
    startActivity(intent2);
  }
}
