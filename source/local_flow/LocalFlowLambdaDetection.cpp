/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowLambdaDetection.h>

#include <string>

namespace marianatrench {
namespace local_flow {

bool LambdaDetection::is_kotlin_lambda_class(std::string_view type_name) {
  // Kotlin Function interfaces: Lkotlin/jvm/functions/Function0; through
  // Lkotlin/jvm/functions/Function22; and FunctionN
  static constexpr std::string_view prefix = "Lkotlin/jvm/functions/Function";
  if (type_name.find(prefix) == std::string_view::npos) {
    return false;
  }

  // Extract the suffix after "Function"
  auto pos = type_name.find(prefix);
  auto suffix_start = pos + prefix.size();
  if (suffix_start >= type_name.size()) {
    return false;
  }

  // Check if suffix is a number (0-22) or "N" followed by ";"
  auto suffix = type_name.substr(suffix_start);
  if (suffix.starts_with("N;") || suffix.starts_with("N$")) {
    return true;
  }

  // Check for numeric suffix (0-22)
  std::string num_str;
  for (auto ch : suffix) {
    if (ch >= '0' && ch <= '9') {
      num_str += ch;
    } else {
      break;
    }
  }

  if (num_str.empty()) {
    return false;
  }

  auto num = std::stoi(num_str);
  return num >= 0 && num <= 22;
}

bool LambdaDetection::is_lambda_invoke(const DexMethodRef* method_ref) {
  auto method_name = std::string(method_ref->get_name()->str());
  if (method_name != "invoke") {
    return false;
  }

  auto* class_type = method_ref->get_class();
  if (class_type == nullptr) {
    return false;
  }

  auto type_name = std::string(class_type->str());
  return is_kotlin_lambda_class(type_name);
}

} // namespace local_flow
} // namespace marianatrench
