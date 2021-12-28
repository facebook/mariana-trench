/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;

public class AppLinksUtil {

  public Intent generateFacewebFallbackIntent(
      CustomUriIntentMapper mapper, Context context, String facewebUri) {
    Intent intent = mapper.getIntentForUri(context, facewebUri);
    if (intent != null) {
      intent.putExtra("aaa", facewebUri);
      intent.putExtra("application_link_type", "applink_navigation_event");
    }
    return intent;
  }
}
