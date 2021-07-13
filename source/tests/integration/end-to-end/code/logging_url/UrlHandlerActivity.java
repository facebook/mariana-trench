/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.instagram.url;

import android.app.Activity;
import android.content.Intent;
import android.text.TextUtils;
import com.instagram.common.analytics.IgAnalyticsLogger;
import com.instagram.common.analytics.intf.AnalyticsEvent;

public class UrlHandlerActivity extends Activity {

  private static final String SHORT_URL = "short_url";

  @Override
  protected void onCreate() {
    super.onCreate();
    handleIntent(getIntent());
  }

  private void handleIntent(Intent intent) {
    String url = intent.getDataString();

    // simplified from a more complicated condition
    // if (UrlHandlerHelper.getInstance().canHandle(url, mSession)) { ...
    if (url.contains("a")) {
      reportUrlEvent(url, intent.getStringExtra(SHORT_URL));
    }
  }

  private void reportUrlEvent(String url, String shortUrl) {
    // TODO (T47487087): we should detect a flow on this line but not yet
    AnalyticsEvent event = AnalyticsEvent.obtain("ig_url_loaded").addExtra("url", url);
    if (!TextUtils.isEmpty(shortUrl)) {
      event.addExtra("short_url", shortUrl);
    }
    event.addExtra("fbid", "123");
    event.addExtra("fb_installed", "123");
    event.addExtra("waterfall_id", "123");
    IgAnalyticsLogger.getInstance().reportEvent(event);
  }
}
