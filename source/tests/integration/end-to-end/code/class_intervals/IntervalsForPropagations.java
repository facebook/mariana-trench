/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class IntervalsForPropagations {
  abstract class Base {
    public abstract Object maybePropagation(Object o);

    // Expected: callee_interval=interval(DerivedWithPropagation), preserves_type_context=true
    public Object polymorphicPropagation(Object o) {
      return this.maybePropagation(o);
    }
  }

  class DerivedWithPropagation extends Base {
    // Expected: callee_interval=interval(DerivedWithPropagation), preserves_type_context=true
    public Object maybePropagation(Object o) {
      return o;
    }
  }

  class DerivedWithoutPropagation extends Base {
    public Object maybePropagation(Object o) {
      return new Object();
    }
  }

  class Unrelated {
    public void issueViaBase(Base base) {
      Object source = Origin.source();
      // Expected: Base.polymorphicPropagation has preserves_type_context=true.
      // Interval(base) will be intersected with interval(DerivedWithPropgation).
      // The intersection is non-empty, which retains the propagation and produces an issue.
      Object propagatedSource = base.polymorphicPropagation(source);
      Origin.sink(propagatedSource);
    }

    public void issueViaDerived(DerivedWithPropagation derived) {
      Object source = Origin.source();
      Object propagatedSource = derived.polymorphicPropagation(source);
      Origin.sink(propagatedSource);
    }

    public void noIssue(DerivedWithoutPropagation derived) {
      Object source = Origin.source();
      // Expected: Base.polymorphicPropagation has preserves_type_context=true.
      // Interval(DerivedWithoutPropagation) will be intersected with
      // interval(DerivedWithPropgation). The intersection is empty, so the propagation will be
      // dropped and the issue should not be emitted.
      Object propagatedSource = derived.polymorphicPropagation(source);
      Origin.sink(propagatedSource);
    }
  }
}
