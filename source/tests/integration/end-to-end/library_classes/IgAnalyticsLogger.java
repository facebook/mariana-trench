// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.instagram.common.analytics;

import com.instagram.common.analytics.intf.AnalyticsEvent;

public class IgAnalyticsLogger {

  public static IgAnalyticsLogger getInstance() {
    return new IgAnalyticsLogger();
  }

  public void reportEvent(AnalyticsEvent event) {}
}
