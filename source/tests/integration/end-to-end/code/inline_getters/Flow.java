/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  private Flow foo;
  private Flow bar;

  public Flow getThis() {
    return this;
  }

  public Flow getFoo() {
    return this.foo;
  }

  public Flow getBar() {
    return this.bar;
  }

  public Flow getFooBar() {
    return this.foo.bar;
  }

  public Flow getBarFoo() {
    return this.bar.foo;
  }

  public void simpleIssue() {
    getThis().foo = (Flow) Origin.source();
    Origin.sink(getThis().foo);
  }

  public void simpleNonIssue() {
    getThis().foo = (Flow) Origin.source();
    Origin.sink(getThis().bar);
  }

  public void otherSimpleIssue() {
    getFoo().bar = (Flow) Origin.source();
    Origin.sink(getFoo().bar);
  }

  public void otherSimpleNonIssue() {
    getFoo().bar = (Flow) Origin.source();
    Origin.sink(getBar().foo);
    Origin.sink(getBar().bar);
  }

  public void complexIssue() {
    bar.getFooBar().foo = (Flow) Origin.source();
    Origin.sink(getBarFoo().getBarFoo());
  }

  public void complexNonIssue() {
    bar.getFooBar().foo = (Flow) Origin.source();
    Origin.sink(getFooBar().getFooBar());
  }
}
