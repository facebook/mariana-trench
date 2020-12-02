// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import javax.annotation.Nullable;

public class ExternalLinkUriIntentBuilder extends UriIntentBuilder {

  static final String HOSTNAME = "extbrowser";

  public ExternalLinkUriIntentBuilder() {}

  @Override
  @Nullable
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
