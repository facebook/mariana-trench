/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench;

import static org.assertj.core.api.Assertions.assertThat;

import org.junit.Test;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;

public class MethodHandlerVisitorTest {
  @Test
  public void test() {
    byte[] inputClass = BytecodeProcessing.readBytecode(MethodHandlerVisitorTestClass.class);
    ClassReader reader = new ClassReader(inputClass);
    ClassWriter writer = new ClassWriter(0);
    MethodHandleVisitor methodHandleVisitor = new MethodHandleVisitor(writer);
    reader.accept(methodHandleVisitor, 0);

    String inputClassString = BytecodeProcessing.bytecodeMethodsToString(inputClass);
    String inputClassStringDesugared =
        inputClassString.replace(
            "INVOKEVIRTUAL java/lang/invoke/MethodHandle.invokeExact (Ljava/lang/String;CC)Ljava/lang/String;",
            "POP\n    POP\n    POP");
    String outputClassString = BytecodeProcessing.bytecodeMethodsToString(writer.toByteArray());
    assertThat(inputClassStringDesugared).isEqualTo(outputClassString);
  }
}
