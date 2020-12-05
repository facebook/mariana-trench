/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;
import javax.annotation.Nullable;

public abstract class UriIntentBuilder {

  public @Nullable Intent getIntentForUri(Context context, String uri) {
    return null;
  }
}
