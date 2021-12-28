/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class SinkWebViewClient extends FacebookWebViewClient {

  @Override
  public void onPageFinished(String url) {
    Origin.sink(url);
  }
}
