/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

// Regression test for issue #91: a taint that flows through a *static* field
// must be detected, just like one that flows through an instance field.
//
// `source()` and `sink()` are attached as a source / sink by models.json.
// The two methods below are structurally identical; the only difference is
// whether the carrier field is static. Before the fix, only
// flowThroughInstanceField() reports an issue (the false negative from #91).
public class FlowThroughStaticField {

  static class Holder {
    public static String sStaticField;
    public String mInstanceField;
  }

  String source() {
    return "tainted";
  }

  void sink(String data) {}

  // Bug case from issue #91: source -> STATIC field -> sink.
  void flowThroughStaticField() {
    String foo = source();
    Holder.sStaticField = foo; // sput-object
    sink(Holder.sStaticField); // sget-object
  }

  // Control case that already works: source -> INSTANCE field -> sink.
  void flowThroughInstanceField() {
    String foo = source();
    Holder holder = new Holder();
    holder.mInstanceField = foo; // iput-object
    sink(holder.mInstanceField); // iget-object
  }
}
