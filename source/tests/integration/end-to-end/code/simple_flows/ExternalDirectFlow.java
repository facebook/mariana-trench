/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;
import android.content.Intent;

public class ExternalDirectFlow {

  public void flow(Activity activity) {
    Intent intent = activity.getIntent();
    activity.startActivity(intent);
  }
}
