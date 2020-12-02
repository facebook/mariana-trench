// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import com.facebook.secure.context.SecureContextHelperV2;

public class OfferRenderingUtils {

  private SecureContextHelperV2 mSecureContextHelper;

  public OfferRenderingUtils() {
    mSecureContextHelper = new SecureContextHelperV2();
  }

  public void navigateToStoreURL(Context context, String externalUrl) {
    Intent intent = new Intent();
    intent.setData(Uri.parse(externalUrl));
    mSecureContextHelper.inAppBrowser().launchActivity(intent, context);
  }
}
