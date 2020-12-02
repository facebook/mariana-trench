// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package android.app;

import android.content.Context;
import android.content.Intent;

public class Activity extends Context {

  protected void onCreate() {}

  public Intent getIntent() {
    return new Intent();
  }

  @Override
  public void startActivity(Intent intent) {}
}
