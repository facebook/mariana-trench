/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.instagram.common.analytics;

import com.instagram.common.analytics.intf.AnalyticsEvent;

public class IgAnalyticsLogger {

  public static IgAnalyticsLogger getInstance() {
    return new IgAnalyticsLogger();
  }

  public void reportEvent(AnalyticsEvent event) {}
}
