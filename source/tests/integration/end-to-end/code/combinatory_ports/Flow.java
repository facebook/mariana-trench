/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  /* Combinatory explosion of ports using overrides. */

  private interface Base {
    void method();
  }

  private class A implements Base {
    private Base a;

    public void method() {
      this.a.method();
    }
  }

  private class B implements Base {
    private Base b;

    public void method() {
      this.b.method();
    }
  }

  private class C implements Base {
    private Base c;
    private Object x;

    public void method() {
      Origin.sink(this.x);
      this.c.method();
    }
  }

  /* Combinatory explosion using recursion. */

  private class Node {
    private Node parent;
    private Object x;

    public void recursive_parent() {
      if (parent != null) {
        this.parent.recursive_parent();
      } else {
        Origin.sink(this);
      }
    }

    public void recursive_parent_x() {
      if (parent != null) {
        this.parent.recursive_parent_x();
      } else {
        Origin.sink(this.x);
      }
    }
  }
}
