/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package kotlin.jvm.internal;

public class Intrinsics {
  private Intrinsics() {}

  public static void checkNotNull(Object object) {
    if (object == null) {
      throw new RuntimeException();
    }
  }

  public static void checkNotNull(Object object, String message) {
    if (object == null) {
      throw new RuntimeException();
    }
  }

  public static void checkExpressionValueIsNotNull(Object value, String expression) {
    if (value == null) {
      throw new RuntimeException();
    }
  }

  public static void checkNotNullExpressionValue(Object value, String expression) {
    if (value == null) {
      throw new RuntimeException();
    }
  }

  public static void checkReturnedValueIsNotNull(
      Object value, String className, String methodName) {
    if (value == null) {
      throw new RuntimeException();
    }
  }

  public static void checkReturnedValueIsNotNull(Object value, String message) {
    if (value == null) {
      throw new RuntimeException();
    }
  }

  public static void checkFieldIsNotNull(Object value, String className, String fieldName) {
    if (value == null) {
      throw new RuntimeException();
    }
  }

  public static void checkFieldIsNotNull(Object value, String message) {
    if (value == null) {
      throw new RuntimeException();
    }
  }

  public static void checkParameterIsNotNull(Object value, String paramName) {
    if (value == null) {
      throw new RuntimeException();
    }
  }

  public static void checkNotNullParameter(Object value, String paramName) {
    if (value == null) {
      throw new RuntimeException();
    }
  }

  public static void checkHasClass(String internalName) {
    throw new RuntimeException();
  }

  public static void checkHasClass(String internalName, String requiredVersion) {
    throw new RuntimeException();
  }

  public static void throwNpe() {
    throw new RuntimeException();
  }

  public static void throwNpe(String message) {
    throw new RuntimeException();
  }

  public static void throwJavaNpe() {
    throw new RuntimeException();
  }

  public static void throwJavaNpe(String message) {
    throw new RuntimeException();
  }

  public static void throwUninitializedProperty(String message) {
    throw new RuntimeException();
  }

  public static void throwUninitializedPropertyAccessException(String propertyName) {
    throw new RuntimeException();
  }

  public static void throwAssert() {
    throw new RuntimeException();
  }

  public static void throwAssert(String message) {
    throw new RuntimeException();
  }

  public static void throwIllegalArgument() {
    throw new RuntimeException();
  }

  public static void throwIllegalArgument(String message) {
    throw new RuntimeException();
  }

  public static void throwIllegalState() {
    throw new RuntimeException();
  }

  public static void throwIllegalState(String message) {
    throw new RuntimeException();
  }

  public static void throwUndefinedForReified() {
    throw new RuntimeException();
  }

  public static void throwUndefinedForReified(String message) {
    throw new RuntimeException();
  }
}
