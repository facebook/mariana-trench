/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.example.myapplication;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

public class MainActivity extends Activity {

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    final Intent intent = getIntent();
    setContentView(R.layout.activity_main);

    final Button button = findViewById(R.id.button);
    button.setOnClickListener(
        new View.OnClickListener() {
          public void onClick(View v) {
            CodeExecuteUtil.execute(intent.getStringExtra("command"));
          }
        });

    launchFallback(intent);
  }

  public void launchFallback(Intent intent) {
    if (intent.getBooleanExtra("redirect", false)) {
      Intent redirectIntent = new Intent();
      redirectIntent.setData(intent.getData());
      if (intent.hasExtra("component")) {
        try {
          Class name = Class.forName("component");
          redirectIntent.setClass(this, name);
        } catch (Exception e) {

        }
      }
      startActivity(redirectIntent);
    }
  }
}
