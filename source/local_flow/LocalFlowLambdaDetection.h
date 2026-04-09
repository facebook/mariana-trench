/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string_view>

class DexMethodRef;

namespace marianatrench {
namespace local_flow {

/**
 * Detects Kotlin lambda types and lambda invocations for higher-order
 * flow labeling (HoCall/HoReturn/Capture).
 *
 * Kotlin compiles lambdas to classes implementing
 * kotlin.jvm.functions.FunctionN. When these are invoked via
 * FunctionN.invoke(...), we use HoCall/HoReturn labels. When they are captured
 * (constructor args), we use Capture labels.
 */
class LambdaDetection {
 public:
  /**
   * Check if the given type implements a Kotlin Function interface
   * (kotlin.jvm.functions.Function0 through Function22, or FunctionN).
   */
  static bool is_kotlin_lambda_class(std::string_view type_name);

  /**
   * Check if a method reference is a FunctionN.invoke(...) call.
   */
  static bool is_lambda_invoke(const DexMethodRef* method_ref);
};

} // namespace local_flow
} // namespace marianatrench
