/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
