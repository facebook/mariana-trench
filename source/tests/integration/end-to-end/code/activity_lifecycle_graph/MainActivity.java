/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;
import android.os.Bundle;

public class MainActivity extends Activity {
  private Object mSource;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    // Expect issue
    Origin.sink(mSource);
  }

  @Override
  protected void onStart() {
    mSource = Origin.source();
  }
}
