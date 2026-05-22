/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench;

import static org.assertj.core.api.Assertions.assertThat;

import java.io.IOException;
import org.junit.Test;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;

public class NestVisitorTest {
  @Test
  public void test() throws IOException {
    byte[] outerClass = BytecodeProcessing.readBytecode(NestVisitorTestClass.class);
    byte[] innerClass = BytecodeProcessing.readBytecode(NestVisitorTestClass.InnerClass.class);

    ClassReader reader = new ClassReader(outerClass);
    ClassWriter writer = new ClassWriter(/* no flags */ 0);
    NestVisitor nestVisitor = new NestVisitor(writer);
    reader.accept(nestVisitor, /* no options */ 0);
    String outerClassString = BytecodeProcessing.bytecodeClassToString(outerClass);
    String outerClassDesugaredString =
        BytecodeProcessing.bytecodeClassToString(writer.toByteArray());

    reader = new ClassReader(innerClass);
    writer = new ClassWriter(/* no flags */ 0);
    nestVisitor = new NestVisitor(writer);
    reader.accept(nestVisitor, /* no options */ 0);
    String innerClassString = BytecodeProcessing.bytecodeClassToString(innerClass);
    String innerClassDesugaredString =
        BytecodeProcessing.bytecodeClassToString(writer.toByteArray());

    assertThat(
            outerClassString.replace(
                "  NESTMEMBER com/facebook/marianatrench/NestVisitorTestClass$InnerClass\n", ""))
        .withFailMessage(outerClassString + "\n" + outerClassDesugaredString)
        .isEqualTo(outerClassDesugaredString);

    assertThat(
            innerClassString.replace(
                "  NESTHOST com/facebook/marianatrench/NestVisitorTestClass\n", ""))
        .withFailMessage(innerClassString + "\n" + innerClassDesugaredString)
        .isEqualTo(innerClassDesugaredString);
  }
}
