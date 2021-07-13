/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.Intent;

/**
 * Test case for CRTEX consumer cases. Sources have callee_ports marked
 * "Producer:<id>:<canonical_port>". CRTEX consumer sinks not covered yet as that is a less common
 * scenario.
 */
public class Consumer {
  /* Models CRTEX parameter source on argument `data`. */
  protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    Origin.sink(data);
  }

  /* Models CRTEX generation source on the returned value. */
  protected int getCrtexField() {
    return 0;
  }

  /* Verifies that trace frame leads to `getCrtexField`. */
  protected int postcondition() {
    return this.getCrtexField();
  }
}
