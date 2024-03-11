/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.Intent;
import android.net.Uri;
import androidx.fragment.app.FragmentActivity;
import com.facebook.content.SecureContextHelper;

class TestUri extends Uri {}

public class RecorderActivity extends FragmentActivity {
  private SecureContextHelper mSecureContextHelper;

  public void flow() {
    Intent intent = new Intent("test", new TestUri());
    mSecureContextHelper.startFacebookActivityForResult(intent, 0, this);
  }

  // This is to test validation checks on via_type_of ports in the model.
  // In models.json Origin.source has an invalid via_type_of port, so the via-type feature does not
  // show up on the issue. Additionally, you can manually check the logs for errors due to a
  // non-existent port being provided to via_type_of
  public void badViaTypeOfPort() {
    Origin.sink(Origin.source());
  }

  private static void sinkViaClass(Object source, Class klass) {}

  public void reflectionViaTypeOfPort() {
    sinkViaClass(Origin.source(), TestUri.class);
  }
}
