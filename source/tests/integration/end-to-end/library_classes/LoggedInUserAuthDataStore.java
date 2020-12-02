// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.auth.datastore;

import com.facebook.user.model.User;

public class LoggedInUserAuthDataStore {

  public boolean isLoggedIn() {
    return false;
  }

  public User getLoggedInUser() {
    return new User();
  }
}
