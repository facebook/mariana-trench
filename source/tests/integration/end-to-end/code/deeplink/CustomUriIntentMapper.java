// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;
import javax.annotation.Nullable;

public class CustomUriIntentMapper {

  public CustomUriIntentMapper() {}

  /**
   * Provides a mapping from uris to intents
   *
   * @param context context from which the intent is being obtained
   * @param uri a uri to be opened
   * @return an intent which handles the given uri
   */
  @Nullable
  public Intent getIntentForUri(Context context, String uri) {
    throw new UnsupportedOperationException("not implemented");
  }
}
