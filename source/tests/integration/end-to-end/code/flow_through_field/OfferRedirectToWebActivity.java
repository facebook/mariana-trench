/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.net.Uri;
import android.os.Bundle;
import androidx.fragment.app.FragmentActivity;

public class OfferRedirectToWebActivity extends FragmentActivity {

  private static final String OFFER_URL = "site_uri";

  private String mOfferURL;
  private OfferRenderingUtils mOfferRenderingUtils;

  @Override
  public void onActivityCreate() {
    super.onActivityCreate();

    mOfferRenderingUtils = new OfferRenderingUtils();

    Bundle extras = getIntent().getExtras();
    mOfferURL = Uri.decode(extras.getString(OFFER_URL));

    mOfferRenderingUtils.navigateToStoreURL(this, mOfferURL);

    navigateToBrowser();
  }

  private void navigateToBrowser() {
    mOfferRenderingUtils.navigateToStoreURL(this, mOfferURL);
  }
}
