/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.webkit.WebViewClient;

public class SecureWebViewClient {

  private final WebViewClient mWebViewClientFallback;

  public SecureWebViewClient() {
    mWebViewClientFallback = new WebViewClient();
  }

  public void onPageFinished(String url) {
    mWebViewClientFallback.onPageFinished(url);
  }
}
