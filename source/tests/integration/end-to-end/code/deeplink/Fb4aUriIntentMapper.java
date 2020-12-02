// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
