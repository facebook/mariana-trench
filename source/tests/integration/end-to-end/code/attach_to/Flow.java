/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  private Object field;

  public Object attach_to_return_sources() {
    return Origin.source();
  }

  public void attach_to_this_sources() {
    this.field = Origin.source();
  }

  public void attach_to_sinks(Object x) {
    Origin.sink(x);
  }

  public Object attach_to_return_propagations(Object x) {
    return x;
  }

  public void attach_to_this_propagations(Object x) {
    this.field = x;
  }

  public Object attach_to_argument_propagations(Object x) {
    return x;
  }

  public void add_features_to_arguments(Object x) {}

  interface Obscure {
    public Object add_features_to_arguments_on_obscure(Object x);
  }

  public void issue(Obscure obscure) {
    Object x = attach_to_return_sources();
    Object y = attach_to_return_propagations(x);
    add_features_to_arguments(y);
    Object z = obscure.add_features_to_arguments_on_obscure(y);
    attach_to_sinks(z);
  }
}
