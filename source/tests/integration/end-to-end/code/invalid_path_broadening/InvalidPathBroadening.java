/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

class ObjectA {
  public Object mField1;
}

class ObjectB {
  public Object mField2;
}

class Dictionary {
  // User-defined model: propagation from argument(1) -> argument(0)[<argument(1)>]
  // Has "no-collapse-on-propagation" to avoid "via-propagation-broadening".
  public void set(String key, Object value) {}
}

public class InvalidPathBroadening {
  // Has user-configured "taint-in-taint-out" mode.
  // This simulates obscure methods (no code available to analyze).
  // Also has "no-collapse-on-propagation" to avoid "via-propagation-broadening"
  public static ObjectB taintInTaintOut(ObjectA a) {
    return new ObjectB();
  }

  public static ObjectB dropInvalidPath() {
    ObjectA objA = new ObjectA();
    objA.mField1 = Origin.source();

    // Propagates argument0 to return, so objB.mField1 is tainted.
    ObjectB objB = taintInTaintOut(objA);

    // ObjectB does not have mField1, "Return.mField1" is not a valid path and should be broadened
    // with "via-invalid-path-broadening".
    return objB;
  }

  public static Object keepInvalidPath() {
    ObjectA objA = new ObjectA();
    objA.mField1 = Origin.source();

    // Propagates argument0 to return, so objB.mField1 is tainted.
    ObjectB objB = taintInTaintOut(objA);

    // ObjectB does not have mField1, but the return type is Object so broadening should not happen.
    // mField1 is a valid field in some derived class of Object. Currently a bug.
    return (Object) objB;
  }

  public static Object keepValidPath() {
    ObjectA objA = new ObjectA();
    objA.mField1 = Origin.source();

    // ObjectA has mField1, but the return type is Object. Broadening should not happen. The
    // analysis only looks at the return type and cannot differentiate between the scenario here
    // and the one in `keepInvalidPath()`. We choose to be conservative and not broaden in the
    // absence of additional information.
    return (Object) objA;
  }

  public static Dictionary keepDictionaryPaths() {
    Dictionary dictionary = new Dictionary();

    // objA.mField1 is tainted.
    ObjectA objA = new ObjectA();
    objA.mField1 = Origin.source();

    // dictionary["key"].mField1 is tainted
    dictionary.set("key", objA);

    // return[key].mField1 is tainted
    // mField1 should not be dropped. The type of dictionary["key"] is not resolved during
    // invalid path broadening, so precision should be maintained. Currently a bug.
    return dictionary;
  }
}
