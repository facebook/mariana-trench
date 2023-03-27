/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class B extends Base {
  public Dict propagate(Dict input) {
    Dict output = new Dict();
    String f4 = input.f4;
    String f5 = input.f5;
    String f6 = input.f6;
    output.f0 = f4.concat(f5).concat(f6);
    return output;
  }
}
