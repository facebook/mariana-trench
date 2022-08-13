/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.marianatrench.integrationtests;

public class ExtendedTests {
  TaintedFieldClass tito(TaintedFieldClass taint) {
    return taint;
  }

  void sinkOnField(TaintedFieldClass taint) {
    // Issue reported here for field Source
    Origin.sink(taint.field);
  }

  // ----------------------------------------------------------------
  //  Additional taint on field and sink via getter.
  // ----------------------------------------------------------------
  public void testAdditionalTaintViaGetter() {
    TaintedFieldClass taintedFieldClass = new TaintedFieldClass();

    taintedFieldClass.field = Origin.source();

    // Expected to have two issues here.
    Origin.sink(taintedFieldClass.get());
  }

  // ----------------------------------------------------------------
  //  Additional taint on field and sink via field access.
  // ----------------------------------------------------------------
  public void testAdditionalTaintViaFieldAssign() {
    TaintedFieldClass taintedFieldClass = new TaintedFieldClass();

    taintedFieldClass.field = Origin.source();

    // Expected to have two issues here. FN: Issue with only Source but not with OriginSource
    Origin.sink(taintedFieldClass.field);
  }

  public void testAdditionalTaintViaSetter() {
    TaintedFieldClass taintedFieldClass = new TaintedFieldClass();

    taintedFieldClass.put(Origin.source());

    // Expected to have two issues here. FN: Issue with only Source but not with OriginSource
    Origin.sink(taintedFieldClass.field);
  }

  public void testAdditionalTaintViaObscureSetter() {
    TaintedFieldClass taintedFieldClass = new TaintedFieldClass();

    // Obscure with propagation: Argument(1) --> Argument(0).field
    taintedFieldClass.putObscure(Origin.source());

    // Expected to have two issues here. FN: Issue with only Source but not with OriginSource
    Origin.sink(taintedFieldClass.field);
  }

  // ----------------------------------------------------------------
  //  Field model and additional taint through tito and
  //  sink via field access
  // ----------------------------------------------------------------
  public void testAdditionalTaintViaTito() {
    TaintedFieldClass taintedFieldClass = new TaintedFieldClass();

    taintedFieldClass.field = Origin.source();
    // No Issue expected here.
    Origin.sink(taintedFieldClass.noTaint);

    taintedFieldClass = tito(taintedFieldClass);

    // Expected to have issue with OriginSource because of tito collapse.
    Origin.sink(taintedFieldClass.noTaint);

    // Expected to have two issues here.
    Origin.sink(taintedFieldClass.field);
  }

  // Additional taint via tito and sink instance.
  public void testAdditionalTaintViaTitoAndSinkInstance() {
    TaintedFieldClass taintedFieldClass = new TaintedFieldClass();

    taintedFieldClass.put(Origin.source());

    // Issue with OriginSource only.
    // No issue expected with field Source as `.field` has not been accessed.
    Origin.sink(taintedFieldClass);

    taintedFieldClass = tito(taintedFieldClass);

    // Issue with OriginSource only.
    // No issue expected with field Source as `.field` has not been accessed.
    Origin.sink(taintedFieldClass);
  }

  // ----------------------------------------------------------------
  //  Sinks on argument field access paths.
  // ----------------------------------------------------------------
  public void testSinkOnFieldSource() {
    TaintedFieldClass taintedField = new TaintedFieldClass();

    // No issue expected.
    // The issue with field Source is expected to be reported in
    // sinkOnField() itself.
    sinkOnField(taintedField);
  }

  public void testSinkOnFieldWithAdditionalSource() {
    TaintedFieldClass taintedField = new TaintedFieldClass();
    taintedField.field = Origin.source();

    // Issue with OriginSource only.
    // The issue with field Source is expected to be reported in
    // sinkOnField() itself.
    sinkOnField(taintedField);
  }
}
