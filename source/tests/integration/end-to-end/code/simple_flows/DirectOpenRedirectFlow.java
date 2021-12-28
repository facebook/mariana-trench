/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import androidx.fragment.app.FragmentActivity;
import com.facebook.auth.datastore.LoggedInUserAuthDataStore;
import com.facebook.base.activity.FragmentBaseActivityUtil;
import com.facebook.content.SecureContextHelper;
import com.facebook.user.model.User;

// Open-redirect flow as seen in S158206
public class DirectOpenRedirectFlow extends FragmentActivity {

  @Override
  protected void onActivityCreate() {
    super.onActivityCreate();

    Intent incomingIntent = getIntent();
    Bundle bundle = incomingIntent.getExtras();

    String upsellImpressionId = null;
    if (bundle != null) {
      upsellImpressionId = bundle.getString("app_growth_impression_id");
    }

    Uri u = incomingIntent.getData();

    SecureContextHelper secureContextHelper = new SecureContextHelper();

    String extraRedirect = u.getQueryParameter("landing_page");
    Intent redirIntent = null;

    if (extraRedirect != null && !extraRedirect.isEmpty()) {
      redirIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(extraRedirect));
    }
    String userId = u.getQueryParameter("contactpoint");

    LoggedInUserAuthDataStore authDataStore = new LoggedInUserAuthDataStore();
    if (authDataStore != null && authDataStore.isLoggedIn()) {
      User currentUser = authDataStore.getLoggedInUser();

      if (currentUser != null && userId.equals(currentUser.getId())) {
        FragmentBaseActivityUtil fragmentBaseActivityUtil = new FragmentBaseActivityUtil();
        // same userId
        Intent nextStartIntent =
            (redirIntent != null
                ? redirIntent
                : fragmentBaseActivityUtil.getDefaultActivityIntent());

        secureContextHelper.startFacebookActivity(nextStartIntent, this);
      }
    }
  }
}
