/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import java.net.URLEncoder;

public class KindSpecificSanitizers {

  private static void sinkRCE(Object obj) {}

  private static void sinkSSRF(Object obj) {}

  public static int parseInt(String s) throws NumberFormatException {
    return 0;
  }

  public static void no_err1() {
    // Argument Propagation
    String eventId = (String) Origin.source();
    try {
      parseInt(eventId);
    } catch (Exception e) {
      // We don't care about the exception details, the point is that it gets
      // thrown so that eventId is effectively sanitized
      return;
    }
    // FP, `parseInt` should drop all taints if there is no exception
    Origin.sink(eventId);
  }

  public static void no_err2() {
    String eventId = (String) Origin.source();
    String uriEncodedTaint = URLEncoder.encode(eventId); // Safe for SSRF sinks
    // FP, `URLEncoder.encode` should drop the SSRF taint
    sinkSSRF(uriEncodedTaint);
  }

  public static void err3() {
    String eventId = (String) Origin.source();
    String uriEncodedTaint = URLEncoder.encode(eventId);
    sinkRCE(uriEncodedTaint); // Not Safe for RCE sinks
  }

  public static void err4() {
    String eventId = (String) Origin.source();
    String uriEncodedTaint = uriEncodeWrapper(eventId);
    sinkRCE(uriEncodedTaint); // Not Safe for RCE sinks
  }

  public static String uriEncodeWrapper(String s) {
    return URLEncoder.encode(s);
  }
}
