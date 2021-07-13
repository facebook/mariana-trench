/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;

public class StaticUriIntentMapper extends CustomUriIntentMapper {

  private static final String SEPERATOR = "://";

  public StaticUriIntentMapper() {}

  @Override
  public Intent getIntentForUri(Context context, String uri) {
    String prefix = getHostname(uri);
    Intent intent = LegacyUriMap.getIntent(context, prefix, uri);
    return intent;
  }

  public static String getHostname(String uri) {
    if (uri == null || !uri.contains(SEPERATOR)) {
      return uri;
    }
    int start = uri.indexOf(SEPERATOR) + SEPERATOR.length();
    int len = uri.length();
    // truncate at the first special character
    for (int i = start; i < len; i++) {
      char c = uri.charAt(i);
      if (c == '/' || c == '?' || c == '=') {
        return uri.substring(start, i);
      }
    }
    return uri.substring(start);
  }
}
