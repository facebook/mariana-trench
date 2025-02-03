/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class AliasingObjects {
  static class MyClass {
    String myString;
    Object myObject;
  }

  public void falseNegative() {
    //   new-instance MyClass;  move-result-object v0
    // v0 -> Root(new-inst1)
    MyClass myInstance = new MyClass();

    //   invoke-static Origin.source();  move-result-object v1
    // v1 -> Root(invoke-static)
    //   iput-object src=v1, dst=v0 MyClass;.myObject
    // Root(new-inst1) -> .myObject -> {Root(invoke-static)}
    myInstance.myObject = Origin.source();

    //   iget-object v0 MyClass;.myObject;  move-result-object v1
    // v1 -> FieldMemLoc(Root(new-inst1), .myObject)
    // Alias analysis does not dereference on iget. v1 is tracked as a FieldMemLoc
    // and only resolved when accessed.
    Object myObject = myInstance.myObject;

    //   new-instance MyClass;  move-result-object v2
    // v2 -> Root(new-inst2)
    //   iput-object src=v2, dst=v0 MyClass;.myObject
    // Root(new-inst1) -> .myObject -> {Root(new-inst2)}
    myInstance.myObject = new MyClass();

    //   invoke-static v1 Origin.sink()
    // Read: v1 -> FieldMemLoc(Root(new-inst1), .myObject)
    // Resolves to: Root(new-inst2), rather than Root(invoke-static), hence the false negative.
    Origin.sink(myObject);
  }
}
