/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

class ConfigApi {
  boolean getFlag(long specifier) {
    return false;
  }
}

class ScopedConfigApi extends ConfigApi {}

class ConfigReader {
  boolean readConstant() {
    return new ConfigApi().getFlag(1234L);
  }

  boolean readDynamic(long specifier) {
    return new ConfigApi().getFlag(specifier);
  }
}

public class CallChainViaFeatures {
  // One issue, with `via-value:5678` materialized at the call effect sink itself.
  static void entryDirectConstant() {
    new ConfigApi().getFlag(5678L);
  }

  // One issue. The feature is materialized one hop away, in `readConstant`, and
  // must survive being propagated up the call chain to here.
  static void entryTransitiveConstant() {
    new ConfigReader().readConstant();
  }

  // One issue, with `via-value:unknown` because the specifier is not a constant.
  static void entryDynamic(long specifier) {
    new ConfigReader().readDynamic(specifier);
  }

  // One issue. The sink is declared on `ConfigApi`, so `via-type` must report the
  // more precise receiver type at the call site rather than the declaring type.
  static void entryReceiverSubtype() {
    new ScopedConfigApi().getFlag(9012L);
  }
}
