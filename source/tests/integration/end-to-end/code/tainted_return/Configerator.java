// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import android.net.Uri;

public class Configerator {

  private ZeroRatingConnectionConfigOverrides mZeroRatingConnectionConfigOverrides;

  private Uri mConfigUri;

  public synchronized void addConfigOverrides(
      ZeroRatingConnectionConfigOverrides connectionConfigOverrides) {
    mZeroRatingConnectionConfigOverrides = connectionConfigOverrides;
  }

  public void configure() {
    mConfigUri = mZeroRatingConnectionConfigOverrides.getUri();
  }

  public Uri getConfigUrl() {
    return mConfigUri;
  }
}
