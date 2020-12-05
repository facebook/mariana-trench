/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.instagram.common.analytics.intf;

import javax.annotation.Nullable;

public class AnalyticsEvent {

  public static AnalyticsEvent obtain(String name) {
    return new AnalyticsEvent();
  }

  public AnalyticsEvent addExtra(String key, @Nullable String value) {
    return this;
  }
}
