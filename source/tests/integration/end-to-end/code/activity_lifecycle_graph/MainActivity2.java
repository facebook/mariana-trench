/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;
import android.os.Bundle;

public class MainActivity2 extends Activity {
  private Object mSource;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    mSource = Origin.source();
  }

  @Override
  protected void onStart() {
    // Expect issue
    Origin.sink(mSource);
  }
}
