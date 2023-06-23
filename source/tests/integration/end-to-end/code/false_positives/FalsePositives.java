/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class FalsePositives {

  // not expected to see a flow
  public final void fp1_unexpected(UserBlob u) {
    u.setF1("Not_tainted");
    Origin.sink(u.getF1());
  }

  // expected to see a flow
  public final void fp1_expected(UserBlob u) {
    u.setF1("Not_tainted");
    Origin.sink(u.getF2());
  }
}

class UserBlob {
  String f1;
  String f2;
  
  public String getF1() {
    return f1;
  }
  
  public void setF1(String s) {
    f1 = s;
  }

  public String getF2() {
    return f2;
  }
  
  public void setF2(String s) {
    f2 = s;
  }
}
