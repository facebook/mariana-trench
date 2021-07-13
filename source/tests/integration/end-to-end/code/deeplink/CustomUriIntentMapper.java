/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;

public class CustomUriIntentMapper {

  public CustomUriIntentMapper() {}

  /**
   * Provides a mapping from uris to intents
   *
   * @param context context from which the intent is being obtained
   * @param uri a uri to be opened
   * @return an intent which handles the given uri
   */
  public Intent getIntentForUri(Context context, String uri) {
    throw new UnsupportedOperationException("not implemented");
  }
}
