/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.xapp.messaging.modularsync.fbn.stax;

import com.facebook.staxcqlinterfacejava.StaxParticipantList;

public final class FbnStaxParticipantList implements StaxParticipantList {
  @Override
  public Integer getContactTypeExact(int row) {
    return null;
  }

  @Override
  public int getCount() {
    return 1;
  }
}
