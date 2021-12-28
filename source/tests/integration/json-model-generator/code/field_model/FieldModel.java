/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class FieldModel {
  String field_one;

  @TestFieldAnnotation("TestValue")
  Boolean field_two;

  @TestFieldAnnotation("Value")
  Integer field_three;
}
