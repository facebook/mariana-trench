/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.quicklog;

public interface EventBuilder {
  EventBuilder annotate(String key, int value);

  EventBuilder annotate(String key, boolean value);

  EventBuilder annotate(String key, String value);

  void report();
}
