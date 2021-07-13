/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
