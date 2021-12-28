/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.os.Bundle;
import androidx.fragment.app.FragmentActivity;

public class BaseFragment extends FragmentActivity {
  protected void onBeforeActivityCreate() {}

  protected void onActivityCreate() {}

  protected void onAfterActivityCreate() {}

  @Override
  protected final void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    onBeforeActivityCreate();
    onActivityCreate();
    onAfterActivityCreate();
  }

  @Override
  protected void onStart() {
    super.onStart();
  }
}
