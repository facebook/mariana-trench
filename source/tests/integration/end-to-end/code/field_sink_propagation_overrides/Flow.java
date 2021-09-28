/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  public class Base {}

  public class WithField extends Base {
    public Object field;
  }

  // Contains `field` due to inheritance
  public class WithField2 extends WithField {}

  // Does not have `field`, but has `irrelevantField`.
  public class IrrelevantField extends Base {
    public Object irrelevantField;
  }

  public void toSinkWithField(WithField thing) {
    Origin.sink(thing.field);
  }

  public WithField taintResult(Base thing) {
    return new WithField();
  }

  public WithField taintResultField(Base thing) {
    WithField f = new WithField();
    f.field = thing;
    return f;
  }

  public IrrelevantField source() {
    return new IrrelevantField();
  }

  // Analysis should find that withField.field can lead to the sink. This is
  // correct.
  public void toSink(WithField2 withField) {
    WithField propagated = taintResult(withField);
    toSinkWithField(propagated);
  }

  // Analysis will find that irrelevantField.field leads to sink, but this field
  // does not exist.
  public void toSinkUnreachablePort(IrrelevantField irrelevantField) {
    WithField propagated = taintResult(irrelevantField);
    toSinkWithField(propagated);
  }

  // This false positive is a result of incorrectly inferred taint in
  // toSinkUnreachablePort.
  public void falsePositive() {
    toSinkUnreachablePort(source());
  }

  // Analysis will find that irrelevantField.field leads to sink, but it should
  // be irrelevantField (arg1) that flows into the sink.
  public void toSinkWrongCallerPort(IrrelevantField irrelevantField) {
    // Initial: irrelevantField -> AbstractTree{`arg1` -> ArtificalSource}
    // taintResultField propagation model:
    //   arg1 -> return (defined in models) [*]
    //   arg1 -> return.`field` (inferred) [**]

    WithField propagated = taintResultField(irrelevantField);
    // Without [*]: propagated -> arg1 -> `field` -> ArtificalSource
    // ^^ Analysis will correctly infer that `irrelevantField` leads to sink.
    //
    // With    [*]: propagated -> join(
    //   from [*]:  arg1 -> ArtificalSource,
    //   from [**]: arg1 -> `field` -> ArtificalSource
    // ) = arg1 -> ArtificalSource
    // ^^ Causes analysis to infer that `irrelevantField.field` leads to sink.

    toSinkWithField(propagated);
  }

  // This false negative is a result of incorrectly inferred taint in
  // toSinkWrongCallerPort.
  public void falseNegative() {
    IrrelevantField irrelevantField = new IrrelevantField();
    irrelevantField.irrelevantField = Origin.source();
    toSinkWrongCallerPort(irrelevantField);
  }
}
