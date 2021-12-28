/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench;

import com.google.common.io.ByteStreams;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.Type;
import org.objectweb.asm.tree.ClassNode;
import org.objectweb.asm.tree.MethodNode;
import org.objectweb.asm.util.Printer;
import org.objectweb.asm.util.Textifier;
import org.objectweb.asm.util.TraceClassVisitor;
import org.objectweb.asm.util.TraceMethodVisitor;

public class BytecodeProcessing {
  public static byte[] readBytecode(Class<?> clazz) {
    try (InputStream input =
        clazz.getResourceAsStream("/" + Type.getType(clazz).getInternalName() + ".class")) {
      return ByteStreams.toByteArray(input);
    } catch (IOException exception) {
      throw new RuntimeException(exception);
    }
  }

  public static String bytecodeMethodsToString(byte[] bytecode) {
    ClassReader reader = new ClassReader(bytecode);
    ClassNode node = new ClassNode();
    reader.accept(node, /* no options */ 0);
    Printer printer = new Textifier();
    TraceMethodVisitor visitor = new TraceMethodVisitor(printer);
    for (MethodNode methodNode : node.methods) {
      methodNode.accept(visitor);
    }
    StringWriter writer = new StringWriter();
    printer.print(new PrintWriter(writer));
    return writer.toString();
  }

  public static String bytecodeClassToString(byte[] bytecode) {
    ClassReader reader = new ClassReader(bytecode);

    StringWriter writer = new StringWriter();
    PrintWriter printWriter = new PrintWriter(writer);
    TraceClassVisitor visitor = new TraceClassVisitor(printWriter);
    reader.accept(visitor, /* no options */ 0);

    return writer.toString();
  }
}
