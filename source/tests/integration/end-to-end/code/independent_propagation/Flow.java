// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.marianatrench.integrationtests;

public class Flow {
  private class Event {
    public Object a;
    public Object b;
    public Object c;

    public void propagate() {}
  }

  private class EventPropagateOne extends Event {
    public void propagate() {
      // `this.b = this.a` added in models.json
    }
  }

  private class EventPropagateTwo extends Event {
    public void propagate() {
      // `this.c = this.b` added in models.json
    }
  }

  private class EventPropagateThree extends Event {
    public void propagate() {
      // `this.b = this.a` added in models.json
    }
  }

  public void issue(Event event) {
    event.b = Origin.source();
    event.propagate();
    Origin.sink(event.c);
  }

  public void non_issue(Event event) {
    event.a = Origin.source();
    event.propagate();
    Origin.sink(event.c);
  }
}
