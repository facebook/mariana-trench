/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.os.Bundle;
import androidx.fragment.app.FragmentActivity;

public class SimpleFragment extends FragmentActivity {
  private String mTainted;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    mTainted = (String) Origin.source();
  }

  @Override
  public void onStart() {
    super.onStart();
    Origin.sink(mTainted);
  }
}
