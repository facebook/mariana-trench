/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.telemetry.ueinterface;

public interface UnexpectedEventBuilder {
  UnexpectedEventBuilder annotate(String key, int value);

  UnexpectedEventBuilder annotate(String key, boolean value);

  UnexpectedEventBuilder annotate(String key, String value);

  void report();
}
