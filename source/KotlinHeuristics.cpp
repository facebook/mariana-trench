/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <string>
#include <unordered_set>

#include <IROpcode.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/KotlinHeuristics.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Redex.h>

namespace marianatrench {

namespace {

static const std::unordered_set<std::string> k_instrinsics_check_not_null_method_signatures = {
    "Lkotlin/jvm/internal/Intrinsics;.checkExpressionValueIsNotNull:(Ljava/lang/Object;Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.checkFieldIsNotNull:(Ljava/lang/Object;Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.checkFieldIsNotNull:(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.checkHasClass:(Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.checkHasClass:(Ljava/lang/String;Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.checkNotNull:(Ljava/lang/Object;)V",
    "Lkotlin/jvm/internal/Intrinsics;.checkNotNull:(Ljava/lang/Object;Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.checkNotNullExpressionValue:(Ljava/lang/Object;Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.checkNotNullParameter:(Ljava/lang/Object;Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.checkParameterIsNotNull:(Ljava/lang/Object;Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.checkReturnedValueIsNotNull:(Ljava/lang/Object;Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.checkReturnedValueIsNotNull:(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/String;)V"};

static const std::unordered_set<std::string> k_intrinsics_throw_method_signatures = {
    "Lkotlin/jvm/internal/Intrinsics;.throwAssert:()V",
    "Lkotlin/jvm/internal/Intrinsics;.throwAssert:(Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.throwIllegalArgument:()V",
    "Lkotlin/jvm/internal/Intrinsics;.throwIllegalArgument:(Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.throwIllegalState:()V",
    "Lkotlin/jvm/internal/Intrinsics;.throwIllegalState:(Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.throwJavaNpe:()V",
    "Lkotlin/jvm/internal/Intrinsics;.throwJavaNpe:(Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.throwNpe:()V",
    "Lkotlin/jvm/internal/Intrinsics;.throwNpe:(Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.throwParameterIsNullIAE:(Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.throwParameterIsNullNPE:(Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.throwUndefinedForReified:()V",
    "Lkotlin/jvm/internal/Intrinsics;.throwUndefinedForReified:(Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.throwUninitializedProperty:(Ljava/lang/String;)V",
    "Lkotlin/jvm/internal/Intrinsics;.throwUninitializedPropertyAccessException:(Ljava/lang/String;)V"};

class KotlinUtil final {
 public:
  explicit KotlinUtil() {
    // Note that the DexMethod for these may or may not be present depending
    // on whether the kotlin jars are included or not (eg. integration tests).
    // We can create the DexMethod here if it doesn't exist, but this means
    // that the corresponding marianatrench::Method type will not have been
    // loaded in the factory. Since the kotlin-runtime is strictly necessary
    // to execute the kotlin code, here we will assume that the required
    // kotlin internal methods are present in the redex's store. If not
    // available, we will log and fall back to treating it as an obscure
    // method. This means that for integration-tests, that rely on these
    // kotlin internals, we will have to provide a mock implementation (same
    // as we do with android libraries).
    for (const auto& signature :
         k_instrinsics_check_not_null_method_signatures) {
      if (const auto* dex_method = redex::get_method(signature)) {
        intrinsics_check_not_null_methods.emplace(dex_method);
      } else {
        WARNING(
            3,
            "Kotlin heuristics based on `instrinsics_check_not_not_methods` will not be applied. Missing method: {}",
            signature);
      }
    }

    for (const auto& signature : k_intrinsics_throw_method_signatures) {
      if (const auto* dex_method = redex::get_method(signature)) {
        intrinsics_throw_methods.emplace(dex_method);
      } else {
        WARNING(
            3,
            "Kotlin heuristics based on `intrinsics_throw_methods` will not be applied. Missing method: {}",
            signature);
      }
    }
  }

 public:
  std::unordered_set<const DexMethod*> intrinsics_check_not_null_methods;
  std::unordered_set<const DexMethod*> intrinsics_throw_methods;
};

const KotlinUtil& get_kotlin_util_singleton() {
  static const KotlinUtil kotlin_util{};
  return kotlin_util;
}

} // namespace

bool KotlinHeuristics::skip_parameter_type_overrides(const DexMethod* callee) {
  return get_kotlin_util_singleton().intrinsics_check_not_null_methods.count(
             callee) != 0;
}

bool KotlinHeuristics::method_has_side_effects(const DexMethod* callee) {
  const auto& kotlin_util = get_kotlin_util_singleton();
  return kotlin_util.intrinsics_check_not_null_methods.count(callee) == 0 &&
      kotlin_util.intrinsics_throw_methods.count(callee) == 0;
}

bool KotlinHeuristics::const_string_has_side_effect(
    const IRInstruction* instruction) {
  mt_assert(opcode::is_const_string(instruction->opcode()));
  // const-string "<set-?>" is generated within setter methods for lateinit var
  // to make sure the objects are initialized correctly.
  return instruction->get_string()->str() != "<set-?>";
}

} // namespace marianatrench
