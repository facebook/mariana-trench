// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
