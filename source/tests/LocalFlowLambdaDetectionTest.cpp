/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/local_flow/LocalFlowLambdaDetection.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;
using namespace marianatrench::local_flow;

namespace {

class LocalFlowLambdaDetectionTest : public test::Test {};

} // namespace

TEST_F(LocalFlowLambdaDetectionTest, DetectsKotlinFunction0) {
  EXPECT_TRUE(
      LambdaDetection::is_kotlin_lambda_class(
          "Lkotlin/jvm/functions/Function0;"));
}

TEST_F(LocalFlowLambdaDetectionTest, DetectsKotlinFunction1) {
  EXPECT_TRUE(
      LambdaDetection::is_kotlin_lambda_class(
          "Lkotlin/jvm/functions/Function1;"));
}

TEST_F(LocalFlowLambdaDetectionTest, DetectsKotlinFunction22) {
  EXPECT_TRUE(
      LambdaDetection::is_kotlin_lambda_class(
          "Lkotlin/jvm/functions/Function22;"));
}

TEST_F(LocalFlowLambdaDetectionTest, DetectsKotlinFunctionN) {
  EXPECT_TRUE(
      LambdaDetection::is_kotlin_lambda_class(
          "Lkotlin/jvm/functions/FunctionN;"));
}

TEST_F(LocalFlowLambdaDetectionTest, RejectsNonLambdaClass) {
  EXPECT_FALSE(LambdaDetection::is_kotlin_lambda_class("Ljava/lang/Object;"));
  EXPECT_FALSE(LambdaDetection::is_kotlin_lambda_class("Lkotlin/Unit;"));
  EXPECT_FALSE(
      LambdaDetection::is_kotlin_lambda_class("Lcom/example/MyClass;"));
}
