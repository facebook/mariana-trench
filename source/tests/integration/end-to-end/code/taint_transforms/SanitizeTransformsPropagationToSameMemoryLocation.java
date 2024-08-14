/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

class SanitizeTransformsPropagationToSameMemoryLocation {
  public Object mNested;

  /**
   * FP case for builder pattern when "alias-memory-location-on-invoke" is present. This mode
   * results in the "Return" port having the same memory location as argument(0), giving the
   * appearance of an arg(0) -> arg(0) propagation, despite the fact that such a propagation is
   * never inferred.
   */
  public static final class Builder {
    int number;

    // Source "builder"
    public Builder() {
      number = 0;
    }

    // Sanitizes sink "build"
    // Has "alias-memory-location-on-invoke" in its model, alongside a propagation from
    // argument0 -> Sanitize[Sink[Build]]:Return.
    public Builder aliasThisOnInvoke(int n) {
      number = n;
      return this;
    }

    // Sink "build"
    public SanitizeTransformsPropagationToSameMemoryLocation build() {
      return new SanitizeTransformsPropagationToSameMemoryLocation();
    }
  }

  // FP: MT should not report an issue here, but it does
  static SanitizeTransformsPropagationToSameMemoryLocation testBuilderNoIssue() {
    // When calling .number(42), given its sanitizer, its return value will be sanitized.
    // Forward analysis will taint the result register with Sanitize[Sink[Build]]@Builder.
    //
    // However, with "alias-memory-location-on-invoke", the result register is aliased to
    // the same location as the register holding the result of Builder;.<init>() (argument(0)),
    // rather than its own unique result register. This other register contains the Builder source
    // and a weak join of Sanitize[Sink[Build]]@Builder with Builder does not remove the
    // existing source.
    return new Builder().aliasThisOnInvoke(42).build();
  }

  /**
   * The next set of tests shows other arg0 -> arg0 propagations without the use of
   * "alias-memory-location-on-invoke". It also verifies that we do not infer such propagations.
   */
  void sanitizeAThis() {
    // Analysis will see the flow:
    //   argument(0).mNested -> Sanitize[Source[A]]:LocalArgument(0)
    // However, arg0 -> [optional sanitizing transforms:]arg0 propagations are dropped.
    this.mNested = SanitizeTransforms.sanitizeSourceA(this.mNested);
  }

  void wrapSanitizeAThis() {
    // If arg0 -> arg0 propagations were inferred, this would also have sanitizer
    // argument(0).mNested -> Sanitize[Source[A]]:LocalArgument(0)
    this.sanitizeAThis();
  }

  void sanitizeASinkIssue() {
    this.mNested = SanitizeTransforms.sourceA();
    // If arg(i) -> arg(i) propagations were inferred, since argument(0).mNested is tainted by
    // SourceA, the resulting kind should be bottom.
    //
    // However, similar to the aliasing problem above, propagations are applied with weak updates.
    // Joining bottom with the existing taint in argument(0), which contains sourceA, is still
    // going to contain sourceA. Since such sanitizing propagations are pointless until the weak
    // update issue is addressed, they will not be inferred/propagated.
    this.wrapSanitizeAThis();
    SanitizeTransforms.sinkB(this);
  }
}
