/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
