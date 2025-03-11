/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench;

import static org.objectweb.asm.Opcodes.ASM9;

import com.facebook.infer.annotation.Nullsafe;
import org.objectweb.asm.ClassVisitor;

@Nullsafe(Nullsafe.Mode.LOCAL)
public class NestVisitor extends ClassVisitor {
  public NestVisitor(ClassVisitor next) {
    super(ASM9, next);
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
