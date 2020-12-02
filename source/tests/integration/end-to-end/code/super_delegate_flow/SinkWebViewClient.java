// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

public class SinkWebViewClient extends FacebookWebViewClient {

  @Override
  public void onPageFinished(String url) {
    Origin.sink(url);
  }
}
