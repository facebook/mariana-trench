/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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

  // Analysis will find that irrelevantField.field leads to sink, but the field
  // does not exist and will be collapsed to just arg1. However, it cannot be
  // dropped unless there is a way to distinguish between this and
  // `toSinkCollapsedPort2` below.
  public void toSinkCollapsedPort(IrrelevantField irrelevantField) {
    WithField propagated = taintResult(irrelevantField);
    toSinkWithField(propagated);
  }

  // This false positive is a result of incorrectly inferred taint in
  // `toSinkCollapsedPort`.
  public void falsePositive() {
    toSinkCollapsedPort(source());
  }

  // Like to `toSinkCollapsedPort` above, arg1.field is collapsed into arg1.
  // Here, this behavior is desired.
  public void toSinkCollapsedPort2(IrrelevantField irrelevantField) {
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

  public void issue() {
    IrrelevantField irrelevantField = new IrrelevantField();
    irrelevantField.irrelevantField = Origin.source();
    toSinkCollapsedPort2(irrelevantField);
  }
}
