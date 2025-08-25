/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;

// Mock base class for testing
abstract class PublicBindableLifecycleServiceWithSwitchOff {
  public abstract IBinder onBind(Intent intent);

  public abstract int onStartCommand(Intent intent, int flags, int startId);
}

public class PublicBindableServiceTest extends PublicBindableLifecycleServiceWithSwitchOff {

  public IBinder onBind(Intent intent) {
    // This method receives external Intent data from third-party apps
    // Should be marked as ServiceUserInput source
    return null;
  }

  public int onStartCommand(Intent intent, int flags, int startId) {
    // This method receives external Intent data
    // Should be marked as ServiceUserInput source
    return 0; // START_NOT_STICKY equivalent
  }

  public void onMessageReceived(MessageEvent messageEvent) {
    // This method receives external message data
    // Should be marked as ServiceUserInput source
  }

  // Helper classes for the test
  public static class MessageEvent {
    private String sourceNodeId;
    private String path;
    private byte[] data;

    public MessageEvent(String sourceNodeId, String path, byte[] data) {
      this.sourceNodeId = sourceNodeId;
      this.path = path;
      this.data = data;
    }
  }

  public static class FileTransferEvent {
    private String sourceNodeId;
    private String path;
    private Uri uri;
    private Bundle metadata;

    public FileTransferEvent(String sourceNodeId, String path, Uri uri, Bundle metadata) {
      this.sourceNodeId = sourceNodeId;
      this.path = path;
      this.uri = uri;
      this.metadata = metadata;
    }
  }
}
