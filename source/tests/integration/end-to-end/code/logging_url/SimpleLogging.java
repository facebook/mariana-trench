/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package simple.logging;

import android.app.Activity;
import android.content.Intent;
import com.instagram.common.analytics.intf.AnalyticsEvent;

public class SimpleLogging extends Activity {

  @Override
  protected void onCreate() {
    super.onCreate();
    Intent intent = getIntent();
    String sensitive = intent.getStringExtra("sensitive");

    AnalyticsEvent event = AnalyticsEvent.obtain("derp").addExtra("url", "http://url");
    event.addExtra("sensitive", sensitive.toString());
  }
}
