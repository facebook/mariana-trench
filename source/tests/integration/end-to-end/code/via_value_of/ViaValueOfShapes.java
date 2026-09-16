/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

class ShapeConfigApi {
  boolean getFlag(long specifier) {
    return false;
  }
}

class ShapeConstants {
  // JLS 13.1 constant variable: javac folds this into every use site.
  static final long FOLDED = 1004L;

  // Not a compile-time constant, so uses emit a real `sget-wide`.
  static final long COMPUTED = computeSeed();

  static long mutable = 1006L;

  static final long[] TABLE = {1015L, 1016L};

  static long computeSeed() {
    return 1005L;
  }
}

class ShapeHolder {
  long specifier;
}

// A value boxed in an object, rather than passed as a primitive.
class ShapeSpecifier {
  final long value;

  ShapeSpecifier(long value) {
    this.value = value;
  }
}

class ShapeBoxedConfigApi {
  boolean getFlagBoxed(ShapeSpecifier specifier) {
    return false;
  }
}

// A holder whose object is built once in `<clinit>` and read back with
// `sget-object` at every use site, which is the dominant shape for
// generated constant holders.
class ShapeSpecifierHolder {
  static final ShapeSpecifier FIELD = new ShapeSpecifier(1041L);
}

// An interface with real implementors, so the call site joins the base model
// with every override model.
interface ShapeConfigContext {
  boolean getContextFlag(long specifier);
}

class ShapeContextA implements ShapeConfigContext {
  @Override
  public boolean getContextFlag(long specifier) {
    return false;
  }
}

class ShapeContextB implements ShapeConfigContext {
  @Override
  public boolean getContextFlag(long specifier) {
    return true;
  }
}

public class ViaValueOfShapes {

  static long constantReturningMethod() {
    return 1007L;
  }

  static void helperTakesSpecifier(long specifier) {
    new ShapeConfigApi().getFlag(specifier);
  }

  // (a) Control: literal directly at the call site.
  static void entryALiteral() {
    new ShapeConfigApi().getFlag(1001L);
  }

  // (b) Constant assigned to a local first.
  static void entryBLocalVariable() {
    long specifier = 1002L;
    new ShapeConfigApi().getFlag(specifier);
  }

  // (c) Join of two constants at a branch.
  static void entryCTernary(boolean condition) {
    new ShapeConfigApi().getFlag(condition ? 1003L : 1013L);
  }

  // (d) `static final long` that javac constant-folds into a `const-wide`.
  static void entryDStaticFinalFolded() {
    new ShapeConfigApi().getFlag(ShapeConstants.FOLDED);
  }

  // (e) `static final long` whose initializer is not a constant expression,
  // so the read is a real `sget-wide`.
  static void entryEStaticFinalComputed() {
    new ShapeConfigApi().getFlag(ShapeConstants.COMPUTED);
  }

  // (f) Mutable static field: `sget-wide`.
  static void entryFStaticMutable() {
    new ShapeConfigApi().getFlag(ShapeConstants.mutable);
  }

  // (g) `move-result-wide` of a method that returns a constant.
  static void entryGMethodReturn() {
    new ShapeConfigApi().getFlag(constantReturningMethod());
  }

  // (h) Specifier arrives at the getter call site as a method parameter.
  static void entryHViaParameterHelper() {
    helperTakesSpecifier(1008L);
  }

  // (i) Arithmetic on a constant: `add-long`.
  static void entryIArithmetic() {
    long base = 1009L;
    new ShapeConfigApi().getFlag(base + 1L);
  }

  // (j) Widening conversion: `int-to-long`.
  static void entryJWidenedInt() {
    int narrow = 1010;
    new ShapeConfigApi().getFlag(narrow);
  }

  // (k) Loop-carried value: the register at the call site is a phi of the
  // initial `const-wide` and the `add-long` from the previous iteration.
  static void entryKLoopCarried() {
    long specifier = 1011L;
    for (int i = 0; i < 3; i++) {
      new ShapeConfigApi().getFlag(specifier);
      specifier = specifier + 1L;
    }
  }

  // (l) Constant stored into an instance field and read back: `iget-wide`.
  static void entryLInstanceField() {
    ShapeHolder holder = new ShapeHolder();
    holder.specifier = 1012L;
    new ShapeConfigApi().getFlag(holder.specifier);
  }

  // (m) Boxed then unboxed, the shape Redex's WrappedPrimitivesPass removes.
  static void entryMBoxedUnboxed() {
    Long boxed = Long.valueOf(1014L);
    new ShapeConfigApi().getFlag(boxed.longValue());
  }

  // (n) Two getter call sites with two distinct constants in one method.
  static void entryNTwoCallSites() {
    new ShapeConfigApi().getFlag(1017L);
    new ShapeConfigApi().getFlag(1018L);
  }

  // (o) Lookup table: `aget-wide`.
  static void entryOArrayLookup(int index) {
    new ShapeConfigApi().getFlag(ShapeConstants.TABLE[index]);
  }

  // (p) Switch that picks one of several constants, then a single call site.
  static void entryPSwitchJoin(int id) {
    long specifier;
    switch (id) {
      case 0:
        specifier = 1019L;
        break;
      case 1:
        specifier = 1020L;
        break;
      default:
        specifier = 1021L;
        break;
    }
    new ShapeConfigApi().getFlag(specifier);
  }

  // (q) Switch with one call site per case, the generated-dispatcher shape.
  static boolean entryQSwitchPerCase(int id) {
    switch (id) {
      case 0:
        return new ShapeConfigApi().getFlag(1022L);
      case 1:
        return new ShapeConfigApi().getFlag(1023L);
      default:
        return new ShapeConfigApi().getFlag(1024L);
    }
  }

  // (r) Interface call with two implementors, so the call-site model is the
  // join of the base model and both override models.
  static void entryRInterfaceDispatch(ShapeConfigContext context) {
    context.getContextFlag(1025L);
  }

  // (s) Specifier boxed in an object constructed at the call site.
  static void entrySBoxedSpecifier() {
    new ShapeBoxedConfigApi().getFlagBoxed(new ShapeSpecifier(1026L));
  }

  // (v) Specifier boxed in an object held in a `static final` field, read with
  // `sget-object`. The shape generated constant holders produce.
  static void entryVBoxedSpecifierFromStaticField() {
    new ShapeBoxedConfigApi().getFlagBoxed(ShapeSpecifierHolder.FIELD);
  }

  // (t) Copy through a second local.
  static void entryTLocalCopy() {
    long first = 1027L;
    long second = first;
    new ShapeConfigApi().getFlag(second);
  }

  // (u) Many call sites in one method.
  static void entryUManyCallSites() {
    ShapeConfigApi api = new ShapeConfigApi();
    api.getFlag(1031L);
    api.getFlag(1032L);
    api.getFlag(1033L);
    api.getFlag(1034L);
    api.getFlag(1035L);
    api.getFlag(1036L);
    api.getFlag(1037L);
    api.getFlag(1038L);
    api.getFlag(1039L);
    api.getFlag(1040L);
  }
}
