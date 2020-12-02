// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;
import android.content.Intent;

public class InheritanceFlows {

  class MyActivity extends Activity {}

  public void flow(MyActivity activity) {
    Intent intent = activity.getIntent();
    activity.startActivity(intent);
  }
}
