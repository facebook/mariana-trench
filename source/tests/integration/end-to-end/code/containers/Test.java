// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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
