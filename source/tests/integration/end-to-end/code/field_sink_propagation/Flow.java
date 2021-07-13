/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  private class Event {
    public Object field;
  }

  public void toSinkWithField(Event event) {
    Origin.sink(event.field);
  }

  public void toSinkWithFieldHop(Event event) {
    toSinkWithField(event);
  }

  public void main() {
    Event event = new Event();
    event.field = Origin.source();
    toSinkWithFieldHop(event);
  }
}
