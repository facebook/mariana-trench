// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;

public class IntentResolver {

  public static boolean isIntentInternallyResolvableToOneComponent(Context context, Intent intent) {
    return intent.getDataString().contains("abc");
  }
}
