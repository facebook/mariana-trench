/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests

class KotlinLambda {
  companion object {
    /**
     * Applies a lambda (Function1) to a value. The invoke() call should produce HoCall/HoReturn
     * labels.
     */
    @JvmStatic
    fun applyTransform(input: Any, transform: (Any) -> Any): Any {
      return transform(input)
    }

    /**
     * Creates a lambda that captures a value. The lambda's constructor args should get Capture
     * labels.
     */
    @JvmStatic
    fun createCapture(captured: Any): () -> Any {
      return { captured }
    }

    /** Chains two lambdas together. Both invoke() calls should produce HoCall/HoReturn labels. */
    @JvmStatic
    fun compose(input: Any, f: (Any) -> Any, g: (Any) -> Any): Any {
      return g(f(input))
    }
  }
}
