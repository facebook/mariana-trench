/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.webkit.WebViewClient;
import java.util.Random;

public class InsecureWebViewClient {

  private final WebViewClient mWebViewClientFallback;

  public InsecureWebViewClient() {
    if (new Random().nextInt(2) == 0) {
      mWebViewClientFallback = new WebViewClient();
    } else {
      mWebViewClientFallback = new SinkWebViewClient();
    }
  }

  public void onPageFinished(String url) {
    mWebViewClientFallback.onPageFinished(url);
  }
}
