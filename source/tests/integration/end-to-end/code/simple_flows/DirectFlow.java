// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;
import android.content.Intent;

public class DirectFlow extends Activity {

  @Override
  protected void onCreate() {
    super.onCreate();
    Intent intent = getIntent();
    startActivity(intent);
  }
}
