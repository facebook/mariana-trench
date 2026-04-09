/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class VirtualDispatch {
  public Object process(Object x) {
    return x;
  }

  public static Object callVirtual(VirtualDispatch obj, Object data) {
    return obj.process(data);
  }
}

class DerivedDispatch extends VirtualDispatch {
  @Override
  public Object process(Object x) {
    return x;
  }
}

interface Processor {
  Object apply(Object input);
}

class ConcreteProcessor implements Processor {
  @Override
  public Object apply(Object input) {
    return input;
  }

  public static Object callInterface(Processor p, Object data) {
    return p.apply(data);
  }
}
