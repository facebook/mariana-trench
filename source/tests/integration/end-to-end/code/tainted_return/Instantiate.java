// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

public class Instantiate {

  protected void onCreate() {
    Configerator config = new Configerator();
    ZeroRatingConnectionConfigOverrides override = new ZeroRatingConnectionConfigOverrides();
    override.onCreate();
    config.addConfigOverrides(override);
    config.configure();
    config.getConfigUrl();
  }
}
