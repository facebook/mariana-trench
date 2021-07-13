/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import android.app.Activity;
import java.util.Map;
import java.util.TreeMap;

public class Test extends Activity {
  protected void onCreate() {
    Map<String, Object> params = new TreeMap<String, Object>();
    params.put("test", Origin.source());
    Origin.sink(params);
  }
}
