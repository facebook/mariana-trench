/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class MainActivityWithBaseImpl extends BaseActivityWithDefaultSink {
  private Object mSource;

  @Override
  protected void onStart() {
    mSource = Origin.source();
  }

  // No issue with super.onStop() since it is skipped in the base.
}
