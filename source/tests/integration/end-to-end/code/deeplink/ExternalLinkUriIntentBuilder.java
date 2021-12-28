/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

public class ExternalLinkUriIntentBuilder extends UriIntentBuilder {

  static final String HOSTNAME = "extbrowser";

  public ExternalLinkUriIntentBuilder() {}

  @Override
  public Intent getIntentForUri(Context context, String uri) {
    Uri parsed = Uri.parse(uri);
    if (FBLinks.SCHEME.equals(parsed.getScheme()) && HOSTNAME.equals(parsed.getHost())) {
      String url = parsed.getQueryParameter("url");
      if (url != null) {
        Uri parsedUri = Uri.parse(url);
        if (parsedUri != null) {
          Intent i = new Intent(Intent.ACTION_VIEW, parsedUri);
          i.putExtra("abc", true);
          return i;
        }
      }
    }
    return null;
  }
}
