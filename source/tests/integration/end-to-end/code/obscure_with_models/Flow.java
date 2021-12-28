/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class Flow {
  public interface IInterface {
    /** The models.json file defines a breadcrumb on this obscure method. */
    abstract int obscure_taint_this_taint_out();

    /** The model_generators.json file defines a breadcrumb on this obscure method. */
    abstract int obscure_taint_this_taint_out_via_model_generator();
  }
}
