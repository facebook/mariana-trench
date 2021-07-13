/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench;

import static org.objectweb.asm.Opcodes.ASM7;
import static org.objectweb.asm.Opcodes.INVOKEVIRTUAL;
import static org.objectweb.asm.Opcodes.POP;

import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.signature.SignatureReader;
import org.objectweb.asm.signature.SignatureVisitor;

public class MethodHandleVisitor extends ClassVisitor {
  public MethodHandleVisitor(ClassVisitor next) {
    super(ASM7, next);
  }

  @Override
  public MethodVisitor visitMethod(
      int access, String name, String desc, String signature, String[] exceptions) {
    return new ProcessVisitMethodInsn(super.visitMethod(access, name, desc, signature, exceptions));
  }

  private static class ProcessVisitMethodInsn extends MethodVisitor {
    public ProcessVisitMethodInsn(MethodVisitor next) {
      super(ASM7, next);
    }

    private static class CountParameters extends SignatureVisitor {
      int mCount;

      public CountParameters() {
        super(ASM7);
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
