/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

interface ActionReceiverLike {
  void onReceive(Object object);
}

interface ActionReceiver {
  void onReceive(ActionReceiverLike receiver, Object object);
}

class BroadcastReceiver implements ActionReceiver {
  @Override
  public void onReceive(ActionReceiverLike receiver, Object object) {
    receiver.onReceive(object);
  }
}

class BroadcastReceiverBuilder {
  void addActionReceiver(ActionReceiver receiver, Object object) {}

  void addActionReceiver(ActionReceiver receiver, ActionReceiverLike receiverLike, Object object) {}
}

public class WithParameterOverrides {
  static void testShimOnAnonymousClass() {
    BroadcastReceiverBuilder builder = new BroadcastReceiverBuilder();
    builder.addActionReceiver(
        new BroadcastReceiver() {
          @Override
          public void onReceive(ActionReceiverLike receiver, Object object) {
            super.onReceive(receiver, object);
            Origin.sink(object);
          }
        },
        Origin.source());
  }

  static void testShimWithAnonymousClassArgument() {
    BroadcastReceiverBuilder builder = new BroadcastReceiverBuilder();
    builder.addActionReceiver(
        new BroadcastReceiver(),
        new ActionReceiverLike() {
          @Override
          public void onReceive(Object object) {
            Origin.sink(object);
          }
        },
        Origin.source());
  }
}
