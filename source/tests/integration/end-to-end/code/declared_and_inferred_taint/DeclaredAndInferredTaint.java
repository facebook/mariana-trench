/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

class DeclaredAndInferredTaint {
  Object declaredSource() {
    return null;
  }

  void declaredOnlyIssue() {
    Object source = this.declaredSource();
    Origin.sink(source);
  }

  Object inferredSource() {
    return this.declaredSource();
  }

  void inferredOnlyIssue() {
    Object source = this.inferredSource();
    Origin.sink(source);
  }

  Object inferredOrDeclaredSource(boolean b) {
    if (b) {
      return this.declaredSource();
    }
    return this.inferredSource();
  }

  void inferredOrDeclaredIssue() {
    Object source = this.inferredOrDeclaredSource(true);
    Origin.sink(source);
  }
}
