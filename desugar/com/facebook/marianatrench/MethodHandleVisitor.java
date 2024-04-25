/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench;

import static org.objectweb.asm.Opcodes.ASM9;
import static org.objectweb.asm.Opcodes.BIPUSH;
import static org.objectweb.asm.Opcodes.INVOKEVIRTUAL;
import static org.objectweb.asm.Opcodes.POP;

import java.util.ArrayList;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Type;
import org.objectweb.asm.signature.SignatureReader;
import org.objectweb.asm.signature.SignatureVisitor;

public class MethodHandleVisitor extends ClassVisitor {
  private boolean mSkipMethodForClass = false;
  private ArrayList<String> mSkippedClasses;

  public MethodHandleVisitor(ClassVisitor next, ArrayList<String> skippedClasses) {
    super(ASM9, next);
    mSkippedClasses = skippedClasses;
  }

  @Override
  public void visit(
      int version,
      int access,
      String name,
      String signature,
      String superName,
      String[] interfaces) {
    for (String skippedClass : mSkippedClasses) {
      // D8 does not run cleanly in some cases. These cases are passed in
      // through a separate configuration. Needs further investigation.
      // Meanwhile, methods in these classes are emptied out to avoid errors
      // in D8.
      if (name.startsWith(skippedClass)) {
        System.out.println("Skipping methods in class: " + name);
        mSkipMethodForClass = true;
      }
    }

    if (!mSkipMethodForClass && name.contains("io/micrometer/")) {
      // For backward compatibility until shim.py, which is responsible for
      // passing in mSkippedClasses as an argument (and should include
      // "io/micrometer/"), is released.
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
        // D8 can only handle MethodHandle;.invoke[Exact] using min API 26
        // (Android 0). We are using min API 25.
        // Therefore, the invoke instruction will be scrubbed. However, the
        // state of the stack needs to be reasonably correct for d8 to infer
        // the type of the invocation result.

        // An invoke instruction corresponding to a handle.invoke(arg1, ...)
        // typically looks like:
        //
        //   // Push parameters onto operand stack
        //   ALOAD 0 // Register for `handle`
        //   ALOAD 1 // Register for `arg1`
        //   ... // Other arg registers. As many as the number of parameters.
        //
        //   // Call the method. Pushes result (if any) onto the operand stack.
        //   INVOKEVIRUTAL java/lang/invoke/MethodHandle.invoke (...)[ReturnType]
        //
        //   // If return type is not void, store the result:
        //   [A|I|...]STORE n

        // Since the invoke instruction is being removed, pop the arguments
        // to restore the operand stack to a state without these arguments.
        SignatureReader reader = new SignatureReader(descriptor);
        CountParameters count = new CountParameters();
        reader.accept(count);
        for (int i = 0; i < count.getCount(); i++) {
          super.visitInsn(POP);
        }

        // If the invoke instruction has a return value, new versions of D8
        // require that the type of the value on the stack matches the type of
        // the store operation. (Was not needed on v29.0.2).
        //
        // At this point, the last thing on the stack is the method handle
        // which is an object/reference type.
        int returnType = Type.getReturnType(descriptor).getSort();
        if (returnType != Type.OBJECT) {
          // Pop the handle reference for non-object return types. The
          // next STORE instruction, if any, will not be an ASTORE.
          // For object return types, the next instruction is an ASTORE
          // and the handle reference is still of the correct type.
          super.visitInsn(POP);

          if (returnType != Type.VOID) {
            // For primitive return types, pushing an integer/byte constant
            // enables the next [I|D|...]STORE operation to work on a value
            // of the correct type.
            super.visitIntInsn(BIPUSH, 42);
          }
        }

        return;
      }

      super.visitMethodInsn(opcode, owner, name, descriptor, isInterface);
    }
  }
}
