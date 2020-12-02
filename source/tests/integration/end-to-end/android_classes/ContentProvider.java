// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package android.content;

import android.database.Cursor;
import android.net.Uri;

public class ContentProvider {
  public Cursor query(
      Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
    return new Cursor();
  }
}
