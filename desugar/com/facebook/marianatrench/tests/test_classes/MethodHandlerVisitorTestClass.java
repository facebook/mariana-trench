/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench;

import android.annotation.SuppressLint;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

// This class is the input of unit tests
public class MethodHandlerVisitorTestClass {
  @SuppressLint("")
  void testMethod() throws Throwable {
    String string;
    MethodType methodType;
    MethodHandle methodHandle;
    MethodHandles.Lookup lookup = MethodHandles.lookup();

    // methodType is (char,char)String
    methodType = MethodType.methodType(String.class, char.class, char.class);
    methodHandle = lookup.findVirtual(String.class, "replace", methodType);
    string = (String) methodHandle.invokeExact("daddy", 'd', 'n');
  }
}
