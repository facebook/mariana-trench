/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench;

import static org.assertj.core.api.Assertions.assertThat;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import org.apache.commons.io.FileUtils;
import org.junit.Test;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;

public class NestVisitorTest {
  @Test
  public void test() throws IOException {
    final String testClassesRelativePath =
        "fbandroid/native/mariana-trench/desugar/com/facebook/marianatrench/tests/test_classes/";
    final String currentWorkingDirectory = System.getProperty("user.dir");

    byte[] outerClass = null;
    byte[] innerClass = null;
    Path classFileDirectory = Files.createTempDirectory("");
    try {
      String classFileDirectoryPath = classFileDirectory.toFile().getCanonicalPath();

      // Compile NestVisitorTestClass.class using javac in JDK11 instead of during tests, because
      // fbandroid appears to not support compilation using Java 11 and hence if we compile them
      // during tests, then the outputs would be Java 8 bytecode
      String[] compileCommand = {
        "bash",
        currentWorkingDirectory + "/fbcode/tools/build/buck/wrappers/javac11.sh",
        "NestVisitorTestClass.java",
        "-d",
        classFileDirectoryPath
      };
      RunCommand.run(
          Runtime.getRuntime()
              .exec(
                  compileCommand,
                  null,
                  Paths.get(currentWorkingDirectory, testClassesRelativePath).toFile()));

      Path outerClassPath =
          Paths.get(
              classFileDirectoryPath, "com/facebook/marianatrench/NestVisitorTestClass.class");
      Path innerClassPath =
          Paths.get(
              classFileDirectoryPath,
              "com/facebook/marianatrench/NestVisitorTestClass$InnerClass.class");
      outerClass = Files.readAllBytes(outerClassPath);
      innerClass = Files.readAllBytes(innerClassPath);
    } finally {
      FileUtils.deleteDirectory(classFileDirectory.toFile());
    }

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
