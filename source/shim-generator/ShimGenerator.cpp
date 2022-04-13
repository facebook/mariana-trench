/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Log.h>
#include <mariana-trench/shim-generator/ShimGenerator.h>

namespace {

// Hardcoded shim definitions.
static const std::unordered_map<std::string, std::vector<std::string>> k_shims = {
    // ------------------------------------------------------------------------
    // For test cases:
    // ------------------------------------------------------------------------
    {/* method to shim */
     "Lcom/facebook/marianatrench/integrationtests/Messenger;.<init>:(Lcom/facebook/marianatrench/integrationtests/Handler;)V",
     /* list of artificial calls */
     {"Lcom/facebook/marianatrench/integrationtests/Handler;.handleMessage:(Ljava/lang/Object;)V"}},
    {/* method to shim */
     "Lcom/facebook/marianatrench/integrationtests/Messenger;.<init>:(Lcom/facebook/marianatrench/integrationtests/Handler;Ljava/lang/Object;)V",
     /* list of artificial calls */
     {"Lcom/facebook/marianatrench/integrationtests/Handler;.handleMessage:(Ljava/lang/Object;)V"}},
    {/* method to shim */
     "Lcom/facebook/marianatrench/integrationtests/FragmentTest;.add:(Landroid/app/Activity;)V",
     /* list of artificial calls */
     {"Landroid/app/Activity;.onCreate:()V"}},
    // ------------------------------------------------------------------------
    // For Android Bound Services using Messenger:
    // https://developer.android.com/guide/components/bound-services#Messenger
    // ------------------------------------------------------------------------
    {/* method to shim */
     "Landroid/os/Messenger;.<init>:(Landroid/os/Handler;)V",
     /* list of artificial calls */
     {"Landroid/os/Handler;.handleMessage:(Landroid/os/Message;)V"}},
    // ------------------------------------------------------------------------
    // For adding a fragment to an activity(non-reflection api)
    // https://developer.android.com/guide/fragments/create#add-programmatic
    // ------------------------------------------------------------------------
    {/* method to shim */
     "Landroidx/fragment/app/FragmentTransaction;.add:(ILandroidx/fragment/app/Fragment;Ljava/lang/String;)Landroidx/fragment/app/FragmentTransaction;",
     /* list of artificial calls */
     {"Landroidx/fragment/app/Fragment;.onCreate:(Landroid/os/Bundle;)V"}},
    {/* method to shim */
     "Landroidx/fragment/app/FragmentTransaction;.add:(ILandroidx/fragment/app/Fragment;)Landroidx/fragment/app/FragmentTransaction;",
     /* list of artificial calls */
     {"Landroidx/fragment/app/Fragment;.onCreate:(Landroid/os/Bundle;)V"}},
};

using namespace marianatrench;

const DexTypeList* MT_NULLABLE get_method_arguments(const Method* method) {
  const auto* method_prototype = method->get_proto();
  if (!method_prototype) {
    return nullptr;
  }

  const auto* dex_arguments = method_prototype->get_args();
  if (!dex_arguments) {
    return nullptr;
  }

  return dex_arguments;
}

std::unordered_map<const DexType*, ShimParameterPosition>
get_parameter_types_to_position(const Method* method) {
  std::unordered_map<const DexType*, ShimParameterPosition> types_to_position;
  ShimParameterPosition index = 0;

  if (!method->is_static()) {
    // Include "this" as argument 0
    types_to_position.emplace(method->get_class(), index++);
  }

  const auto* dex_arguments = get_method_arguments(method);
  if (!dex_arguments) {
    return types_to_position;
  }

  for (const auto* dex_argument : *dex_arguments) {
    types_to_position.emplace(std::make_pair(dex_argument, index++));
  }

  return types_to_position;
}

std::optional<ShimParameterPosition> get_type_position(
    const DexType* dex_type,
    const std::unordered_map<const DexType*, ShimParameterPosition>&
        parameter_types_to_position) {
  auto found = parameter_types_to_position.find(dex_type);
  if (found == parameter_types_to_position.end()) {
    return std::nullopt;
  }

  LOG(5,
      "Found dex type {} in shim parameter position: {}",
      found->first->str(),
      found->second);

  return found->second;
}

std::vector<ShimParameterPosition> get_parameter_positions(
    const Method* method,
    const std::unordered_map<const DexType*, ShimParameterPosition>&
        parameter_types_to_position) {
  std::vector<ShimParameterPosition> parameters;

  const auto* dex_arguments = get_method_arguments(method);
  if (!dex_arguments) {
    return parameters;
  }

  for (const auto* dex_argument : *dex_arguments) {
    if (auto position =
            get_type_position(dex_argument, parameter_types_to_position)) {
      parameters.push_back(*position);
    }
  }

  return parameters;
}

} // namespace

namespace marianatrench {

std::optional<ShimTarget> ShimTarget::try_create(
    const Method* call_target,
    const std::unordered_map<const DexType*, ShimParameterPosition>&
        parameter_types_to_position) {
  if (call_target == nullptr) {
    return std::nullopt;
  }

  LOG(5, "Creating shim target: {}", show(call_target));

  auto receiver_position_in_shim =
      get_type_position(call_target->get_class(), parameter_types_to_position);

  auto parameter_positions_in_shim =
      get_parameter_positions(call_target, parameter_types_to_position);

  return ShimTarget{
      call_target, receiver_position_in_shim, parameter_positions_in_shim};
}

ShimGenerator::ShimGenerator(const std::string& name) : name_(name) {}

std::unordered_map<const Method*, std::vector<ShimTarget>>
ShimGenerator::emit_artificial_callees(const Methods& methods) const {
  std::unordered_map<const Method*, std::vector<ShimTarget>> shims;

  for (const auto& [method_to_shim, artificial_callees] : k_shims) {
    LOG(5, "Method to shim {}", method_to_shim);

    auto* method = methods.get(method_to_shim);
    if (method == nullptr) {
      continue;
    }

    auto parameter_types_to_position = get_parameter_types_to_position(method);
    std::vector<ShimTarget> artificial_call_targets;

    for (const auto& callee : artificial_callees) {
      if (auto shim = ShimTarget::try_create(
              methods.get(callee), parameter_types_to_position)) {
        artificial_call_targets.push_back(*shim);
      }
    }

    if (!artificial_call_targets.empty()) {
      shims.emplace(method, std::move(artificial_call_targets));
    }
  }

  return shims;
}

} // namespace marianatrench
