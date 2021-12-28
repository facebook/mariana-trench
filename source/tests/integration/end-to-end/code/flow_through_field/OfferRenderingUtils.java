/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
