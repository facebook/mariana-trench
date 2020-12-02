// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
