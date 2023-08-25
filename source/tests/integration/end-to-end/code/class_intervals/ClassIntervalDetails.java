/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

/**
 * Tests to verify that details such as the "callee_interval" and "preserves_type_context" flag are
 * populated correctly by the analysis.
 */
public class ClassIntervalDetails {
  class Base {
    // A source declaration. Should contain no class intervals as it is user-defined.
    public Object source() {
      return new Object();
    }

    // Produces an "origin" frame.
    // Expected: callee_interval=interval(Base), preserves_type_context=true
    public Object hop1Base() {
      return this.source();
    }

    // Produces a "callsite" frame.
    // Expected: callee_interval=interval(Base), preserves_type_context=true
    public Object hop2Base() {
      return this.hop1Base();
    }
  }

  class Derived extends Base {
    // Produces an "origin" frame.
    // Expected: callee_interval=interval(Derived), preserves_type_context=true
    public Object hop1Derived() {
      return this.source();
    }

    // Produces a "callsite" frame.
    // Expected: callee_interval=interval(Derived), preserves_type_context=true
    // Verifies that hop1Base's interval is intersected with "this" to give interval(Derived).
    public Object hop2DerivedViaHop1Base() {
      return this.hop1Base();
    }
  }

  class Unrelated {
    // Produces an "origin" frame.
    // Expected: callee_interval=interval(Base), preserves_type_context=false
    public Object hop1ViaReceiverBase(Base receiver) {
      return receiver.source();
    }

    // Produces an "origin" frame.
    // Expected: callee_interval=interval(Derived), preserves_type_context=false
    public Object hop1ViaReceiverDerived(Derived receiver) {
      return receiver.source();
    }

    // Produces a "callsite" frame.
    // Expected: callee_interval=interval(Unrelated), preserves_type_context=true
    // Verifies that despite the callee does not preserve type context, kinds are propagated even if
    // their intervals do not intersect (Unrelated and Base in this case).
    public Object hop2ViaUnrelated() {
      return this.hop1ViaReceiverBase(new Base());
    }
  }
}
