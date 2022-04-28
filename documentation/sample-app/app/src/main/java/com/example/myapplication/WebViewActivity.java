/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.example.myapplication;

import android.app.Activity;
import android.os.Bundle;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import androidx.annotation.Nullable;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;

public class WebViewActivity extends Activity {
  String url = "data:text/html,<p>Hello World</p>";
  String log_urls_name = "log.txt";

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    if (getIntent().hasExtra("incoming_url")) {
      url = getIntent().getStringExtra("incoming_url");
    }
    if (getIntent().hasExtra("log_urls")) {
      log_urls_name = getIntent().getStringExtra("log_urls");
    }

    setContentView(R.layout.activity_web_view);
  }

  @Override
  protected void onStart() {
    super.onStart();

    WebView webview = findViewById(R.id.webview);

    WebViewClient client =
        new WebViewClient() {
          @Nullable
          @Override
          public WebResourceResponse shouldInterceptRequest(
              WebView view, WebResourceRequest request) {
            File file = new File(getCacheDir(), log_urls_name);
            try {
              FileOutputStream stream = new FileOutputStream(file);
              try {
                stream.write(request.getUrl().toString().getBytes(StandardCharsets.UTF_8));
              } finally {
                stream.close();
              }
            } catch (Exception e) {
              return super.shouldInterceptRequest(view, request);
            }

            return super.shouldInterceptRequest(view, request);
          }
        };

    webview.setWebViewClient(client);
    webview.loadUrl(url);
  }
}
