/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.xapp.messaging.threadview.loader.mailbox.adapter.util;

import com.facebook.marianatrench.integrationtests.Origin;
import com.facebook.messengerorcacqljava.OrcaParticipantList;
import com.facebook.staxcqlinterfacejava.StaxParticipantList;
import com.facebook.telemetry.impl.UnexpectedEventBuilderImpl;
import com.facebook.telemetry.ueinterface.UnexpectedEventBuilder;
import com.facebook.xapp.messaging.modularsync.fbn.stax.FbnStaxParticipantList;

public final class ParticipantAdapterUtil {
  private Object getContactType(Integer contactTypeExact) {
    UnexpectedEventBuilder builder = new UnexpectedEventBuilderImpl();
    builder.annotate("contactTypeExact", contactTypeExact.intValue()).report();
    return new Object();
  }

  private Object getParticipantInList(StaxParticipantList participants, int row) {
    return getContactType(participants.getContactTypeExact(row));
  }

  public static void row10() {
    new ParticipantAdapterUtil().getParticipantInList(new OrcaParticipantList(), 0);
  }

  public static void constantNullFbn() {
    new ParticipantAdapterUtil().getParticipantInList(new FbnStaxParticipantList(), 0);
  }

  public static void intPayloadOnly() {
    new UnexpectedEventBuilderImpl().annotate("value", Sources.intSource()).report();
  }

  public static void keyOnly() {
    new UnexpectedEventBuilderImpl().annotate((String) Origin.source(), 0).report();
  }

  public static void booleanPayloadOnly() {
    new UnexpectedEventBuilderImpl().annotate("value", Sources.booleanSource()).report();
  }

  public static void unrelatedReceiverState() {
    UnexpectedEventBuilderImpl builder = new UnexpectedEventBuilderImpl();
    builder.unrelatedState = Origin.source();
    builder.report();
  }
}
