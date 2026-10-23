/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

// Taint stored in a static field must reach its reads, including after a
// clean write: the field is shared mutable state, so updates retain possible
// taint conservatively. This fixture does not execute competing threads.
//
// `source()` and `sink()` are configured in models.json.
// The instance-field case provides a control for ordinary field propagation.
// Both flows use the same source and sink models.
public class FlowThroughStaticField {

  static class Holder {
    public static String sStaticField;
    public String mInstanceField;
  }

  String source() {
    return "tainted";
  }

  void sink(String data) {}

  // Retain potential taint across a clean write to shared static storage.
  void flowThroughStaticField() {
    Holder.sStaticField = source();
    Holder.sStaticField = null;
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
