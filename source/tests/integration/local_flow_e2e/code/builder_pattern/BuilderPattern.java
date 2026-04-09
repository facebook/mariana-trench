/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class BuilderPattern {
  private Object name;
  private Object value;

  public BuilderPattern setName(Object name) {
    this.name = name;
    return this;
  }

  public BuilderPattern setValue(Object value) {
    this.value = value;
    return this;
  }

  public Object getName() {
    return this.name;
  }

  public static BuilderPattern build(Object n, Object v) {
    return new BuilderPattern().setName(n).setValue(v);
  }
}
