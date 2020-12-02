// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;
import javax.annotation.Nullable;

public abstract class UriIntentBuilder {

  public @Nullable Intent getIntentForUri(Context context, String uri) {
    return null;
  }
}
