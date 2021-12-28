/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.os.Bundle;
import android.util.Log;
import androidx.fragment.app.FragmentActivity;

public class MainActivity extends FragmentActivity {

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    Bundle extras = getIntent().getExtras(); // source
    new ModalActivityLauncher(extras)
        // culprit: Call to willHideSystemUi() introduces a propagation and
        // collapsing the input paths taints the entire `this` object.
        // This causes the call to `open()` to assume that
        // `mClickPoint` is tainted when it is not.
        .willHideSystemUi(true)
        .open(); // no issue
  }

  void issue() {
    Bundle extras = getIntent().getExtras();
    new ModalActivityLauncher(extras) // source
        .willHideSystemUi(true)
        .mArgumentFlowsToSink(); // issue
  }

  private class ModalActivityLauncher {
    private final Bundle mArguments;
    private String mClickPoint = "x";
    private boolean mWillHideSystemUi;

    public ModalActivityLauncher(Bundle arguments) {
      this.mArguments = arguments;
    }

    // builder pattern
    public ModalActivityLauncher willHideSystemUi(boolean willHideSystemUi) {
      this.mWillHideSystemUi = willHideSystemUi;
      return this;
    }

    public void open() {
      Log.e("tag", this.mClickPoint);
    }

    public void mArgumentFlowsToSink() {
      Origin.sink(this.mArguments);
    }
  }
}
