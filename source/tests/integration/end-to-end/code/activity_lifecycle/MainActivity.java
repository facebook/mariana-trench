/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.os.Bundle;

public class MainActivity extends BaseActivity {
  private Object mSource;

  @Override
  void doCreate(Bundle savedInstanceState) {
    mSource = Origin.source();
  }

  @Override
  void doStart() {}
}
