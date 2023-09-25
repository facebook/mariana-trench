/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

@interface Path {
  String value();
}

@interface QueryParam {
  String value();
}

public class AnnotationFeature {

  @Path("/issue_1")
  Object getSourceWithMethodAnnotation() {
    return new Object();
  }

  @Path("/issue_2")
  void parameterSourceWithLabel(@QueryParam("query_param_name") String tainted) {
    Origin.sink(tainted);
  }

  Object getSourceWithParameterAnnotation(@QueryParam("query_param_name") String value) {
    return "unrelated";
  }

  void testSourceWithMethodAnnotation() {
    Object source = getSourceWithMethodAnnotation();
    Origin.sink(source);
  }

  void testSourceWithParameterAnnotation() {
    Object source = getSourceWithParameterAnnotation("argument_value");
    Origin.sink(source);
  }
}
