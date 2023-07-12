/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.net.Uri;

public class Test {
  public static void test_one() {
    Uri uri =
        new Uri.Builder().scheme((String) Origin.source()).host("localhost").path("/").build();
    Origin.sink(uri.toString());
  }

  public static void test_two() {
    Uri uri = new Uri.Builder().scheme("http").host((String) Origin.source()).path("/").build();
    Origin.sink(uri.toString());
  }

  public static void test_three() {
    Uri uri =
        new Uri.Builder().scheme("http").host("localhost").path((String) Origin.source()).build();
    Origin.sink(uri.toString());
  }
}
