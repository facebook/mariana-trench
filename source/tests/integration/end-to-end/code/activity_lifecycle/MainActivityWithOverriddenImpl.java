/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class MainActivityWithOverriddenImpl extends BaseActivityWithDefaultSink {
  private Object mSource;

  @Override
  protected void onStart() {
    mSource = Origin.source();
  }

  // No issue because onStop is overridden and does not have a sink.
  @Override
  protected void onStop() {}
}
