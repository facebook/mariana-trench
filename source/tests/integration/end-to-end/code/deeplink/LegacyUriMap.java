// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;

public class LegacyUriMap {

  public static Intent getIntent(Context context, String prefix, String uri) {
    switch (prefix) {
      case "aaa":
        return new ExternalLinkUriIntentBuilder().getIntentForUri(context, uri);
      default:
        return new ExternalLinkUriIntentBuilder().getIntentForUri(context, uri);
    }
  }
}
