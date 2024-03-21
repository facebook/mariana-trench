/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench;

import static org.objectweb.asm.Opcodes.ASM9;
import static org.objectweb.asm.Opcodes.INVOKEVIRTUAL;
import static org.objectweb.asm.Opcodes.POP;

import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.signature.SignatureReader;
import org.objectweb.asm.signature.SignatureVisitor;

public class MethodHandleVisitor extends ClassVisitor {
  private boolean mSkipMethodForClass = false;

  public MethodHandleVisitor(ClassVisitor next) {
    super(ASM9, next);
  }

  @Override
  public void visit(
      int version,
      int access,
      String name,
      String signature,
      String superName,
      String[] interfaces) {
    if (name.contains("io/micrometer/")) {
      // D8 does not run cleanly on this package. Needs further investigation.
      // Meanwhile, remove methods in it.
      System.out.println("Skipping methods in class: " + name);
      mSkipMethodForClass = true;
    }
    super.visit(version, access, name, signature, superName, interfaces);
  }

  @Override
  public MethodVisitor visitMethod(
      int access, String name, String desc, String signature, String[] exceptions) {
    if (mSkipMethodForClass) {
      return null;
    }
    return new ProcessVisitMethodInsn(super.visitMethod(access, name, desc, signature, exceptions));
  }

  private static class ProcessVisitMethodInsn extends MethodVisitor {
    public ProcessVisitMethodInsn(MethodVisitor next) {
      super(ASM9, next);
    }

    private static class CountParameters extends SignatureVisitor {
      int mCount;

      public CountParameters() {
        super(ASM9);
        mCount = 0;
      }

      @Override
      public SignatureVisitor visitParameterType() {
        mCount++;
        return super.visitParameterType();
      }

      public int getCount() {
        return mCount;
      }
    }

    @Override
    public void visitMethodInsn(
        final int opcode,
        final String owner,
        final String name,
        final String descriptor,
        final boolean isInterface) {
      if (opcode == INVOKEVIRTUAL
          && owner.equals("java/lang/invoke/MethodHandle")
          && (name.equals("invoke") || name.equals("invokeExact"))) {
        // Delete the invocation instruction and pop arguments
        // If arguments are not poped, d8 may fail to infer the type of the invocation result
        SignatureReader reader = new SignatureReader(descriptor);
        CountParameters count = new CountParameters();
        reader.accept(count);
        for (int i = 0; i < count.getCount(); i++) {
          super.visitInsn(POP);
        }
        return;
      }
      super.visitMethodInsn(opcode, owner, name, descriptor, isInterface);
    }
  }
}
