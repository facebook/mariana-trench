// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import android.net.Uri;

public class ZeroRatingConnectionConfigOverrides {

  private Uri mUri;

  protected void onCreate() {
    Uri uri = (Uri) Origin.source();
    mUri = uri;
  }

  public Uri getUri() {
    Origin.sink(mUri);
    return mUri;
  }
}
