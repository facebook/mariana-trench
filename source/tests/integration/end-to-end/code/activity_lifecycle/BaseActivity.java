/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;
import android.os.Bundle;

public abstract class BaseActivity extends Activity {

  abstract void doCreate(Bundle savedInstanceState);

  abstract void doStart();

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    this.doCreate(savedInstanceState);
  }

  @Override
  public void onStart() {
    super.onStart();
    this.doStart();
  }
}
