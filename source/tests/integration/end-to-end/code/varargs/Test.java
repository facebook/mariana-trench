/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Test {
  public void secureLog() {
    String username = "mariana-trench";
    Test.log("successfully logged in as user ", username);
  }

  public void insecureLog() {
    String username = "mariana-trench";
    String password = (String) Origin.source();
    Test.log("successfully logged in as user ", username, " with password ", password);
  }

  public void insecureLogTwo() {
    String username = "mariana-trench";
    String password = (String) Origin.source();
    String country = "usa";
    Test.log(
        "successfully logged in as user ",
        username,
        " with password ",
        password,
        " from ",
        country);
  }

  static void log(String... parameters) {
    for (String s : parameters) {
      Origin.sink(s);
    }
  }
}
