/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import androidx.fragment.app.FragmentActivity;

// Open-redirect flow as seen in S158295
public class IntentUriHandler extends FragmentActivity {

  IntentHandlerUtil mIntentHandlerUtil;

  @Override
  protected void onActivityCreate() {
    super.onActivityCreate();
    mIntentHandlerUtil = new IntentHandlerUtil();

    // TODO (T48169442): if everything goes well we should find a flow here
    mIntentHandlerUtil.launchNextActivity(this, getIntent());
  }
}
