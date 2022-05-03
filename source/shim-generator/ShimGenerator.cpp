/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <iterator>

#include <Show.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/constraints/MethodConstraints.h>
#include <mariana-trench/shim-generator/ShimGenerator.h>

namespace marianatrench {
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

std::unordered_map<const DexType*, ShimParameterPosition>
get_parameter_types_to_position(const Method* method) {
  std::unordered_map<const DexType*, ShimParameterPosition> types_to_position;
  ShimParameterPosition index = 0;

  if (!method->is_static()) {
    // Include "this" as argument 0
    types_to_position.emplace(method->get_class(), index++);
  }

  const auto* method_prototype = method->get_proto();
  if (!method_prototype) {
    return types_to_position;
  }

  const auto* dex_arguments = method_prototype->get_args();
  if (!dex_arguments) {
    return types_to_position;
  }

  for (const auto* dex_argument : *dex_arguments) {
    types_to_position.emplace(std::make_pair(dex_argument, index++));
  }

  return types_to_position;
}

} // namespace

ShimGenerator::ShimGenerator(const std::string& name) : name_(name) {}

MethodToShimTargetsMap ShimGenerator::emit_artificial_callees(
    const Methods& methods) const {
  MethodToShimTargetsMap shims;

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

JsonShimGenerator::JsonShimGenerator(
    std::unique_ptr<AllOfMethodConstraint> constraint,
    ShimTemplate shim_template)
    : constraint_(std::move(constraint)),
      shim_template_(std::move(shim_template)) {}

std::optional<Shim> JsonShimGenerator::visit_method(
    const Method* method) const {
  if (constraint_->satisfy(method)) {
    LOG(5,
        "Method `{}{}` satisfies all constraints in shim model generator",
        method->is_static() ? "(static) " : "",
        method->show());
    return shim_template_.instantiate(method);
  }

  return std::nullopt;
}

MethodToShimMap JsonShimGenerator::emit_method_shims(
    const Methods& methods,
    const MethodMappings& method_mappings) {
  MethodToShimMap method_shims;

  MethodHashedSet filtered_methods = constraint_->may_satisfy(method_mappings);
  if (filtered_methods.is_bottom()) {
    return method_shims;
  }

  if (!filtered_methods.is_top()) {
    return visit_methods(
        filtered_methods.elements().begin(), filtered_methods.elements().end());
  }

  return visit_methods(methods.begin(), methods.end());
}

} // namespace marianatrench
