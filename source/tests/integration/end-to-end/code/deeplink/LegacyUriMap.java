/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
