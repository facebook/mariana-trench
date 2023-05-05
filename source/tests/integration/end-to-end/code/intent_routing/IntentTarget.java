/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;
import android.content.Intent;

public class IntentTarget extends Activity {
  static final String SESSION = "session";
  static final String ANOTHER_KEY = "anotherKey";

  // Expected: Intent[session] -> Sink
  void readsRoutedIntent_sessionToSink() {
    Intent intent = this.getIntent();
    Origin.sink((Object) intent.getStringExtra(SESSION));
  }

  // Expected: Intent[session] -> Sink
  // (callee = extraSinkHop, callee_port=arg1)
  void readsRoutedIntent_sessionToSinkViaExtraHop() {
    Intent intent = this.getIntent();
    extraSinkHop((Object) intent.getStringExtra(SESSION));
  }

  // Expected: Intent[anotherKey] -> Sink
  void readsRoutedIntent_anotherKeyToSink() {
    Intent intent = this.getIntent();
    Origin.sink((Object) intent.getStringExtra(ANOTHER_KEY));
  }

  // Expected: Argument(1) -> Sink
  void extraSinkHop(Object object) {
    Origin.sink(object);
  }

  // Propagation: call-effect-intent."path" -> Return
  // Used for testing input access paths in call-effect propagations.
  // We do not have a realistic use-case for this (yet).
  Intent fakeGetIntent() {
    return new Intent();
  }

  // Expected: Intent."path"[session] -> Sink
  void readsRoutedIntent_pathSessionToSink() {
    Intent intent = this.fakeGetIntent();
    Origin.sink((Object) intent.getStringExtra(SESSION));
  }
}
