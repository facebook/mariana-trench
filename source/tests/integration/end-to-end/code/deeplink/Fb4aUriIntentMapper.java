/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;

public class Fb4aUriIntentMapper extends CustomUriIntentMapper {

  private StaticUriIntentMapper mStaticUriIntentMapper;

  public Fb4aUriIntentMapper() {
    mStaticUriIntentMapper = new StaticUriIntentMapper();
  }

  @Override
  public Intent getIntentForUri(Context context, String uri) {
    Intent intent = null;
    if (intent == null) {
      intent = getIntentByUriLookup(context, uri);
    }
    return intent;
  }

  private Intent getIntentByUriLookup(Context context, String uri) {
    Intent intent = mStaticUriIntentMapper.getIntentForUri(context, uri);
    return intent;
  }
}
