// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;
import javax.annotation.Nullable;

public class AppLinksUtil {

  public @Nullable Intent generateFacewebFallbackIntent(
      CustomUriIntentMapper mapper, Context context, String facewebUri) {
    Intent intent = mapper.getIntentForUri(context, facewebUri);
    if (intent != null) {
      intent.putExtra("aaa", facewebUri);
      intent.putExtra("application_link_type", "applink_navigation_event");
    }
    return intent;
  }
}
