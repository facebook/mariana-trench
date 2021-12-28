/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import com.facebook.content.SecureContextHelper;

public class IntentHandlerUtil {

  private AppLinksUtil mAppLinksUtil;
  private CustomUriIntentMapper mUriIntentMapper;
  private SecureContextHelper mSecureContextHelper;

  public IntentHandlerUtil() {
    mAppLinksUtil = new AppLinksUtil();
    mUriIntentMapper = new Fb4aUriIntentMapper();
    mSecureContextHelper = new SecureContextHelper();
  }

  public void launchNextActivity(Context context, Intent incomingIntent) {
    Uri intentUri = incomingIntent.getData();

    if (isShortenedNotificationUri(intentUri)) {
      transformNotificationUri(context, intentUri);
      return;
    }

    // Can't resolve String.valueOf in tests
    // String uri = String.valueOf(intentUri);
    String uri = intentUri.toString();

    String trigger = incomingIntent.getStringExtra("trigger");
    Bundle extra = new Bundle();
    if (uri.startsWith("bbb") && trigger != null && trigger.contains("ccc")) {
      extra.putBoolean("ddd", true);
      extra.putString("trigger", "eee");
    }
    if (uri.startsWith("fff")) {
      extra.putString("trigger", trigger);
    }

    boolean handledUri = handleUri(context, uri, extra);

    if (!handledUri) {
      if (incomingIntent.getExtras() != null && incomingIntent.hasExtra("abc")) {
        Bundle applinksBundle = incomingIntent.getBundleExtra("def");
        if (applinksBundle != null && applinksBundle.containsKey("hij")) {
          String facewebUri = applinksBundle.getString("klm");
          if (facewebUri != null && !facewebUri.isEmpty()) {
            Intent outgoingIntent =
                mAppLinksUtil.generateFacewebFallbackIntent(mUriIntentMapper, context, facewebUri);
            handledUri = handleDestinationIntent(context, facewebUri, outgoingIntent);
          }
        }
      }

      if (!handledUri) {
        handleUri(context, "fb://feed");
      }
    }
  }

  public boolean handleUri(Context context, String uri) {
    return handleUri(context, uri, null, null);
  }

  public boolean handleUri(Context context, String uri, Bundle extras) {
    return handleUri(context, uri, extras, null);
  }

  /**
   * Start an activity for the given uri. For intra-app navigation, you should always use this as
   * opposed to firing off an intent with the appropriate uri. This avoids creating an activity that
   * will just be finish()ed immediately.
   *
   * <p>Returns true if the uri has handled, false if not.
   */
  private boolean handleUri(Context context, String uri, Bundle extras, Intent destinationIntent) {
    if (destinationIntent == null) {
      destinationIntent = mUriIntentMapper.getIntentForUri(context, uri);
    }
    if (destinationIntent == null) {
      return false;
    }
    if (extras != null) {
      destinationIntent.putExtras(extras);
    }
    return handleDestinationIntent(context, uri, destinationIntent);
  }

  /**
   * Handles the actual launching of an intent if possible.
   *
   * @param context The launch context of the Intent
   * @param uri The "dat" uri of the Intent
   * @param intent The Intent to be launched
   * @param extraLoggingParameters Any extra parameters that should be given to listeners
   * @return true if the Intent is launched, false otherwise.
   */
  private boolean handleDestinationIntent(final Context context, String uri, final Intent intent) {
    if (intent == null) {
      return false;
    }

    // Launch the activity
    try {
      if (context instanceof Activity) {
        // Need an activity context to launch another activity
        startActivityDetectInternalOrExternal(context, intent);
      } else {
        // If no activity, need the FLAG_ACTIVITY_NEW_TASK to launch activity from app context
        // This happens when all Activity/Fragment getting dismissed, or launch from
        // push notification or service or Broadcast Receiver
        intent.setFlags(intent.getFlags() | Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivityDetectInternalOrExternal(context, intent);
      }
    } catch (RuntimeException e) {
      return false;
    }
    return true;
  }

  /**
   * Tests where a given uri is a notification uri. The uri must be a proper facebook.com uri and
   * have the path "/n/"
   *
   * @return Returns true if the uri is a notification uri, false if not.
   */
  private boolean isShortenedNotificationUri(Uri intentUri) {
    return true;
  }

  private void transformNotificationUri(final Context context, final Uri notificationUri) {}

  private void startActivityDetectInternalOrExternal(Context context, Intent intent) {
    final boolean hasRequestCode = intent.hasExtra("abc");
    final int requestCode = intent.getIntExtra("def", 0);
    final Activity activity = hasRequestCode ? new Activity() : null;

    if (hasRequestCode && activity != null) {
      startActivityDetectInternalOrExternalForResult(context, intent, requestCode, activity);
    } else {
      startActivityDetectInternalOrExternalImpl(context, intent);
    }
  }

  private void startActivityDetectInternalOrExternalImpl(Context context, Intent intent) {
    if (IntentResolver.isIntentInternallyResolvableToOneComponent(context, intent)) {
      mSecureContextHelper.startFacebookActivity(intent, context);
    } else {
      mSecureContextHelper.startNonFacebookActivity(intent, context);
    }
  }

  private void startActivityDetectInternalOrExternalForResult(
      Context context, Intent intent, final int requestCode, final Activity activity) {
    if (IntentResolver.isIntentInternallyResolvableToOneComponent(context, intent)) {
      mSecureContextHelper.startFacebookActivityForResult(intent, requestCode, activity);
    } else {
      mSecureContextHelper.startNonFacebookActivityForResult(intent, requestCode, activity);
    }
  }
}
