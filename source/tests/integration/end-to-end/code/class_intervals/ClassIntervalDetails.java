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

    // Produces an "origin" frame when callee resolves to Base.source()
    // Expected: callee_interval=interval(Base), preserves_type_context=true
    // Produces a "callsite" frame when callee resolves to Derived.source()
    // Expected: callee_interval=interval(Derived), preserves_type_context=true
    public Object hop1Base() {
      return this.source();
    }

    // Produces "callsite" frames.
    // Expected: callee_interval=interval(Base), preserves_type_context=true
    // Expected: callee_interval=interval(Derived), preserves_type_context=true
    public Object hop2Base() {
      return this.hop1Base();
    }
  }

  class Derived extends Base {
    // Overrides Base.source() but is not a source declaration in our models
    // This would be an "origin" frame. Calls to this.source() in Base can resolve to this
    // and the class intervals should be reflected accordingly.
    // Expected: callee_interval=interval(Derived), preserves_type_context=true
    public Object source() {
      return super.source(); // Leads to a "declaration" frame.
    }

    // Produces an "origin" frame.
    // Expected: callee_interval=interval(Derived), preserves_type_context=true
    public Object hop1Derived() {
      return super.source();
    }

    // Produces a "callsite" frame.
    // Expected: callee_interval=interval(Derived), preserves_type_context=true
    // Verifies that hop1Base's interval is intersected with "this" to give interval(Derived).
    public Object hop2DerivedViaHop1Base() {
      return this.hop1Base();
    }
  }

  class DerivedSibling extends Base {
    // Overrides Base.source().
    // WARNING: NOT ACTUALLY A SOURCE (despite what the name says)
    public Object source() {
      return new Object();
    }
  }

  class Unrelated {
    // Produces an "origin" frame when callee resolves to Base.source().
    // Expected: callee_interval=interval(Unrelated), preserves_type_context=true
    // Any this.* call to this method made from within the class hierarchy of Unrelated would
    // propagate the source.
    //
    // Also produces a "callsite" frame when callee resolves to Derived.source().
    // Expected: callee_interval=interval(Derived), preserves_type_context=false
    // Since preserves_type_context=false, anything that calls this would propagate the source
    // regardless of whether it was a this.* call. See hop2ViaReceiverBase().
    public Object hop1ViaReceiverBase(Base receiver) {
      // This leads to a "declaration" frame in Base.source()
      // and an "origin" frame in Derived.source().
      return receiver.source();
    }

    // Produces a "callsite" frame.
    // Expected: callee_interval=interval(Derived), preserves_type_context=false
    public Object hop2ViaReceiverDerived(Derived receiver) {
      return receiver.hop1Derived();
    }

    // For each call to this.hop1ViaReceiverBase(), the propagated frame interval is the same.
    // Expected: callee_interval=interval(Unrelated), preserves_type_context=true
    //
    // The call leads to an "origin" frame and a "callsite" frame.
    // - The "origin" frame preserves type context and its interval (Unrelated) intersects with
    //   interval(this=Unrelated).
    // - The "callsite" frame does not preserve type context, so it is propagated regardless of
    //   non-intersecting intervals (Derived /\ "this"=Unrelated).
    //
    // Both propagated frames have the same interval which is joined into a single frame.
    public Object hop2ViaReceiverBase(int randomInput) {
      // True positive. Base.source() is a source.
      if (randomInput > 0) {
        return this.hop1ViaReceiverBase(new Base());
      }

      // True positive. Derived.source() is a source.
      if (randomInput < -100) {
        return this.hop1ViaReceiverBase(new Derived());
      }

      // False positive because DerivedSibling.source() is not actually a source (despite
      // the name), but the analysis does not have the information to avoid reporting this.
      return this.hop1ViaReceiverBase(new DerivedSibling());
    }
  }
}
