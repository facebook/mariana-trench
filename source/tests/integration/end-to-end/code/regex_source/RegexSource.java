/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class RegexSource {

  static String TEMPLATE_FIELD;

  static String getAttackerControlledInput() {
    return "";
  }

  static void sink(String query, String aci) {
  }

  // -------------------------------------------------------
  //  Issues
  // -------------------------------------------------------
  public static void testRegexSource() {
    // Expect issue for rules:
    //   (ACI, REGEX) -> Sink
    String query = "SELECT * FROM USERS WHERE id = ";
    String aci = getAttackerControlledInput();
    sink(query, aci);
  }
}
