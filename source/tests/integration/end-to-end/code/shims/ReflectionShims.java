/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

class A {
  void api(Object o) {}
}

class B {
  void api(Object o) {
    Origin.sink(o);
  }
}

public class ReflectionShims {
  private static void reflect(Class klass) {}

  private static void reflectWithArgument(Class klass, Object o) {}

  private static Class getKlass() {
    return A.class;
  }

  public static void testSimple() {
    // At this point klass (v0) is guaranteed to be A.class
    // Shim is created on A.class
    reflect(A.class);
  }

  public static void testSimpleIssue() {
    // At this point klass (v0) is guaranteed to be B.class
    // Shim is created on B.class and the argument is propagated.
    reflectWithArgument(B.class, Origin.source());
  }

  public static void testMultipleCallsites(boolean test) {
    Class klass;
    klass = B.class;
    // At this point klass (v0) is guaranteed to be B.class
    // Shim is created on B.class here: Issue.
    reflectWithArgument(klass, Origin.source());
    klass = A.class;

    // At this point klass (v0) is guaranteed to be A.class
    // Shim is created on A.class here: No issue.
    reflectWithArgument(klass, Origin.source());
  }

  public static void testMultipleWithIssueFN(boolean test) {
    Class klass;
    if (test) {
      klass = A.class;
    } else {
      klass = B.class;
    }

    // At this point klass (v0) can be either A.class || B.class
    // Shim should be on A.class && B.class. But redex's ReflectionAnalysis
    // uses a ConstantAbstractDomain domain, so the join results in top().
    // Shim will not be created.
    reflectWithArgument(klass, Origin.source());
  }

  public static void testHop() {
    Class klass = getKlass();

    // At this point klass (v0) is guaranteed to be A.class
    // but cannot be known with just intraprocedural analysis.
    // Shim will not be created here.
    reflect(klass);
  }
}
