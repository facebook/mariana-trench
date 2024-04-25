/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Retention(RetentionPolicy.RUNTIME)
@interface Path {
  String value();
}

@Retention(RetentionPolicy.RUNTIME)
@interface QueryParam {
  String value();
}

@Retention(RetentionPolicy.RUNTIME)
@interface OtherQueryParam {
  String value();

  String description() default "";
}

@Path("/base_path")
public class AnnotationFeature {

  @Path("/issue_1")
  Object getSourceWithMethodAnnotation() {
    return new Object();
  }

  @Path("/issue_2")
  void parameterSourceWithLabel(@QueryParam("query_param_name") String tainted) {
    Origin.sink(tainted);
  }

  @Path("/issue_3")
  void forAllParameterSourceWithLabel(
      @QueryParam("query_param_1") String param1,
      @OtherQueryParam(value = "other_query_param_1", description = "description_instead_of_value")
          String otherQueryParam1,
      @OtherQueryParam("other_query_param_2") String otherQueryParam2) {
    Origin.sink(otherQueryParam1);
  }

  @Path("/issue_4")
  void parameterSourceWithClassAnnotation(@QueryParam("query_param_2") String param) {
    Origin.sink(param);
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
