/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;

public class IntentResolver {

  public static boolean isIntentInternallyResolvableToOneComponent(Context context, Intent intent) {
    return intent.getDataString().contains("abc");
  }
}
