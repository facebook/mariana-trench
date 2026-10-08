/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.quicklog;

public final class DirectEventBuilder implements EventBuilder {
  public final QuickEventImpl event = new QuickEventImpl();

  @Override
  public EventBuilder annotate(String key, int value) {
    event.mAnnotationsList.annotate(key, value);
    return this;
  }

  @Override
  public EventBuilder annotate(String key, boolean value) {
    event.mAnnotationsList.annotate(key, value);
    return this;
  }

  @Override
  public EventBuilder annotate(String key, String value) {
    event.mAnnotationsList.annotate(key, value);
    return this;
  }

  @Override
  public void report() {}
}
