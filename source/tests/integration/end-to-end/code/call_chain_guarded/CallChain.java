/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

class SensitiveApi {
  void execute() {}
}

class CallChainService {
  void hop1() {
    hop2();
  }

  void hop2() {
    new SensitiveApi().execute();
  }
}

class Exported {
  void entry() {
    new CallChainService().hop1();
  }
}

public class CallChain {
  static void callChainIssue() {
    new Exported().entry();
  }
}
