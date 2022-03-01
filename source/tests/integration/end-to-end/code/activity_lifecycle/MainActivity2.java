/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.Intent;
import android.os.Bundle;

public class MainActivity2 extends BaseActivity {
  private Object mSource;

  @Override
  void doCreate(Bundle savedInstanceState) {
    mSource = new Intent();
  }

  @Override
  void doStart() {
    Origin.sink(mSource);
  }
}
