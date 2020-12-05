/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench;

import static org.objectweb.asm.Opcodes.ASM7;

import org.objectweb.asm.ClassVisitor;

public class NestVisitor extends ClassVisitor {
  public NestVisitor(ClassVisitor next) {
    super(ASM7, next);
  }

  @Override
  public void visitNestHost(final String nestHost) {
    // Delete the host meta-information stored in *.class files
  }

  @Override
  public void visitNestMember(final String nestMember) {
    // Delete the member meta-information stored in *.class files
  }
}
