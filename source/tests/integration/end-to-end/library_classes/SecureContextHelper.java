// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.content;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

public class SecureContextHelper {

  public void startFacebookActivity(Intent intent, Context context) {}

  public void startFacebookActivityForResult(Intent intent, int requestCode, Activity activity) {}

  public void startNonFacebookActivity(Intent intent, Context context) {}

  public void startNonFacebookActivityForResult(
      Intent intent, int requestCode, Activity activity) {}
}
