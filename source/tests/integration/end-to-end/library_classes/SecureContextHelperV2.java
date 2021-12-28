/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.secure.context;

public class SecureContextHelperV2 {

  public SecureContextHelperV2() {}

  public static SecureContextHelperV2 get() {
    return new SecureContextHelperV2();
  }

  public IntentLauncher internal() {
    return new IntentLauncher();
  }

  public IntentLauncher inAppBrowser() {
    return new IntentLauncher();
  }
}
