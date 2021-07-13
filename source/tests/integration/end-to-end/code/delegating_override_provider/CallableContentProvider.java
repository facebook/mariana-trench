/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.database.Cursor;
import android.net.Uri;

public class CallableContentProvider extends AbstractContentProvider {

  @Override
  public final Cursor doQuery(
      Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
    Origin.sink(uri);
    return new Cursor();
  }
}
