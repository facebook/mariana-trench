/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
