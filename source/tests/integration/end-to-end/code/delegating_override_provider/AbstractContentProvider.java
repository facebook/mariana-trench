/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.content.ContentProvider;
import android.database.Cursor;
import android.net.Uri;

public abstract class AbstractContentProvider extends ContentProvider {
  @Override
  public final Cursor query(
      Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
    return doQuery(uri, projection, selection, selectionArgs, sortOrder);
  }

  protected abstract Cursor doQuery(
      Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder);
}
