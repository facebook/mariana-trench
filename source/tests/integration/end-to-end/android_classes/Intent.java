// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package android.content;

import android.net.Uri;
import android.os.Bundle;

public class Intent {

  public static final String ACTION_VIEW = "action.view";
  public static final int FLAG_ACTIVITY_NEW_TASK = 1;

  public Intent() {}

  public Intent(String action, Uri data) {}

  public String getDataString() {
    return "";
  }

  public int getFlags() {
    return 1;
  }

  public void setFlags(int flags) {}

  public boolean hasExtra(String key) {
    return true;
  }

  public String getStringExtra(String key) {
    return "";
  }

  public int getIntExtra(String key) {
    return 0;
  }

  public int getIntExtra(String key, int v) {
    return 0;
  }

  public Uri getData() {
    return new Uri();
  }

  public Bundle getExtras() {
    return new Bundle();
  }

  public Bundle getBundleExtra(String key) {
    return new Bundle();
  }

  public void setData(Uri uri) {}

  public void putExtras(Bundle bundle) {}

  public void putExtra(String key, String value) {}

  public void putExtra(String key, boolean value) {}
}
