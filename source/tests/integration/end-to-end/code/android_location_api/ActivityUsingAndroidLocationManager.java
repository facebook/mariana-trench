/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package android_location_api;

import android.app.Activity;
import android.location.Location;
import android.location.LocationListener;
import android.util.Log;

public class ActivityUsingAndroidLocationManager extends Activity implements LocationListener {

  private Location mLastKnownLoaction;

  @Override
  protected void onCreate() {
    super.onCreate();
  }

  @Override
  public void onLocationChanged(Location location) {
    Log.e("Activity", "fixed string");

    mLastKnownLoaction = location;
    Log.e("Activity", mLastKnownLoaction.toString());

    // Skip this for now in tests because we can't easily add source code for StringBuilder
    // Log.e("Activity", "lat=" + mLastKnownLoaction.getLatitude());
  }
}
