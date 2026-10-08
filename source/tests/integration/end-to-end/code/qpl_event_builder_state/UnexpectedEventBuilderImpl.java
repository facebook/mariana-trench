/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.telemetry.impl;

import com.facebook.quicklog.DirectEventBuilder;
import com.facebook.quicklog.EventBuilder;
import com.facebook.telemetry.ueinterface.UnexpectedEventBuilder;

public final class UnexpectedEventBuilderImpl implements UnexpectedEventBuilder {
  public final EventBuilder eventBuilder = new DirectEventBuilder();
  public Object unrelatedState;

  @Override
  public UnexpectedEventBuilder annotate(String key, int value) {
    eventBuilder.annotate(key, value);
    return this;
  }

  @Override
  public UnexpectedEventBuilder annotate(String key, boolean value) {
    eventBuilder.annotate(key, value);
    return this;
  }

  @Override
  public UnexpectedEventBuilder annotate(String key, String value) {
    eventBuilder.annotate(key, value);
    return this;
  }

  @Override
  public void report() {
    eventBuilder.report();
  }
}
