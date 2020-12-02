// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
