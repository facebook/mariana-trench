// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;
import android.content.Intent;

public class ExternalDirectFlow {

  public void flow(Activity activity) {
    Intent intent = activity.getIntent();
    activity.startActivity(intent);
  }
}
