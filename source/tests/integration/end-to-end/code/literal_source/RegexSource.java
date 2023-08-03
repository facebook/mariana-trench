/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import java.text.MessageFormat;

public class RegexSource {

  static String getAttackerControlledInput() {
    return "";
  }

  static void sink(String query, String aci) {
  }

  // -------------------------------------------------------
  //  Issues
  // -------------------------------------------------------
  public static void testRegexSourceIssue() {
    // Expect issue for rules:
    //   (ACI, REGEX) -> Sink
    String query = "SELECT * FROM USERS WHERE id = ";
    String aci = getAttackerControlledInput();
    sink(query, aci);
  }

  public static void testRegexSourceNoMatchNoIssue() {
    String query = "DELETE FROM USERS WHERE id = ";
    String aci = getAttackerControlledInput();
    sink(query, aci);
  }

  public static void testRegexSourceStringConcatIssue() {
    // Expect issue for rules:
    //   (ACI, REGEX) -> Sink
    String prefix = "SELECT * FROM USERS WHERE id = ";
    String aci = getAttackerControlledInput();
    @SuppressWarnings("unused")
    String query = prefix + aci;
  }

  public static void testRegexSourceStringConcatAciFirstIssue() {
    // Expect issue for rules:
    //   (ACI, REGEX) -> Sink
    String prefix = "SELECT * FROM USERS WHERE id = ";
    String aci = getAttackerControlledInput();
    @SuppressWarnings("unused")
    String query = aci + prefix;
  }

  public static void testRegexSourceStringBuilderIssue() {
    // Expect issue for rules:
    //   (ACI, REGEX) -> Sink
    String prefix = "SELECT * FROM USERS WHERE id = ";
    String aci = getAttackerControlledInput();
    StringBuilder query = new StringBuilder();
    query.append(prefix).append(aci);
  }

  public static void testRegexSourceStringBuilderAciFirstIssue() {
    // Expect issue for rules:
    //   (ACI, REGEX) -> Sink
    String prefix = "SELECT * FROM USERS WHERE id = ";
    String aci = getAttackerControlledInput();
    StringBuilder query = new StringBuilder();
    query.append(aci).append(prefix);
  }

  public static void testRegexSourceStringFormatIssue() {
    // Expect issue for rules:
    //   (ACI, REGEX) -> Sink
    String prefix = "SELECT * FROM USERS WHERE id = %s";
    String aci = getAttackerControlledInput();
    String.format(prefix, aci);
  }

  public static void testRegexSourceMessageFormatIssue() {
    // Expect issue for rules:
    //   (ACI, REGEX) -> Sink
    String aci = getAttackerControlledInput();
    MessageFormat.format("SELECT * FROM USERS WHERE id = {}", aci);
  }
}
