// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
