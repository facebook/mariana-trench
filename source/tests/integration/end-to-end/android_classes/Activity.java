/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package android.app;

import android.content.Context;
import android.content.Intent;

public class Activity extends Context {

  protected void onCreate() {}

  public Intent getIntent() {
    return new Intent();
  }

  @Override
  public void startActivity(Intent intent) {}
}
