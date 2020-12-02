// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
