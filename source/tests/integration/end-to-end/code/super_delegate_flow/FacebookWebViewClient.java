/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.webkit.WebViewClient;

public class FacebookWebViewClient extends WebViewClient {

  @Override
  public void onPageFinished(String url) {
    super.onPageFinished(url);
  }
}
