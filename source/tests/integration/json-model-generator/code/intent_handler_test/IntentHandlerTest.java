/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.Intent;
import android.net.Uri;

// Mock IntentHandler interface for testing
interface IntentHandler {
  Intent getIntent(String authority, Uri uri, String extra);

  void handleIntent(String authority, Uri uri, String extra, Intent intent, String data);
}

public class IntentHandlerTest implements IntentHandler {
  @Override
  public Intent getIntent(String authority, Uri uri, String extra) {
    // This method should have ActivityUserInput source on Argument(2) (extra parameter)
    return new Intent();
  }

  @Override
  public void handleIntent(String authority, Uri uri, String extra, Intent intent, String data) {
    // This method should have ActivityUserInput source on Argument(4) (data parameter)
  }
}
