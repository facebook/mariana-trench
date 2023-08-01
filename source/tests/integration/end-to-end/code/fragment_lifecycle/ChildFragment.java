/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class ChildFragment extends BaseFragment {
  private String mTainted1;
  private String mTainted2;

  protected void onBeforeActivityCreate() {
    mTainted1 = (String) Origin.source();
  }

  protected void onActivityCreate() {}

  protected void onAfterActivityCreate() {
    mTainted2 = (String) Origin.source();
    Origin.sink(mTainted1);
  }

  @Override
  protected void onStart() {
    // False negative:
    // mTainted2 is tainted in onAfterActivityCreate() called from onCreate()
    // but because this class does not override onCreate(), the call to
    // onCreate() does not make it into the life-cycle wrapper and the analysis
    // misses the flow.
    // Suggested fix: Call base class methods when override does not exist.
    // Needs class intervals to avoid false positives because the base class
    // could have tainted fields from other derived classes unrelated to this
    // one.
    super.onStart();
    Origin.sink(mTainted2);
  }
}
