/*
 * Copyright (c) Facebook, Inc. and its affiliates.
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
    super.onStart();
    Origin.sink(mTainted2);
  }
}
