/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  static Object indirect_source() {
    return Origin.source();
  }

  static void indirect_sink(Object toSink) {
    Origin.sink(toSink);
  }

  static void multi_kind_issue() {
    Flow.indirect_sink(Flow.indirect_source());
  }
}
