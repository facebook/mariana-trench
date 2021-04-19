/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package android.app;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

public class Activity extends Context {

  protected void onCreate() {}

  protected void onCreate(Bundle savedInstanceState) {}

  protected void onStart() {}

  public Intent getIntent() {
    return new Intent();
  }

  @Override
  public void startActivity(Intent intent) {}
}
