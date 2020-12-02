// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
