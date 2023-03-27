/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class A extends Base {
  public Dict propagate(Dict input) {
    Dict output = new Dict();
    String f0 = input.f0;
    String f1 = input.f1;
    String f2 = input.f2;
    output.f0 = f0.concat(f1).concat(f2);
    return output;
  }
}
