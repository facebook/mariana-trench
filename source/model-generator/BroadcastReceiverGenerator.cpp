/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <ConcurrentContainers.h>
#include <Walkers.h>
#include <re2/re2.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/CallGraph.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/model-generator/BroadcastReceiverGenerator.h>

namespace marianatrench {

namespace {

// Number of arguments (including implicit this) that we expect in the
// registerReceiver method without the permissions argument
const std::size_t k_register_receiver_number_arguments = 3;

// The source (argument) number of the BroadcastReceiver argument in the
// IRInstruction invoking the registerReceiver method
const std::size_t k_broadcast_receiver_source_position = 1;

// The argument position (including implicit this) of the intent argument in
// BroadcastReceiver.onReceive()
const marianatrench::ParameterPosition k_on_receive_intent_argument_position =
    2;

// The BroadcastReceiver base classes
const std::unordered_set<std::string> k_broadcast_receiver_base_classes = {
    "Landroid/content/BroadcastReceiver;",
    "Lcom/facebook/common/init/BroadcastReceiver;",
    "Lcom/facebook/secure/receiver/ActionReceiver;",
    "Lcom/facebook/content/FacebookOnlySecureBroadcastReceiver;",
};

const std::unordered_set<std::string> k_ignored_register_receiver_signatures = {
    "Landroidx/localbroadcastmanager/content/LocalBroadcastManager;.registerReceiver:(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)V"};

ConcurrentSet<const DexType*> get_broadcast_receiver_types(
    const Context& context,
    const Methods& methods,
    const ConcurrentSet<const Method*>& register_receiver_methods) {
  ConcurrentSet<const DexType*> broadcast_receiver_types;
  auto get_caller_queue =
      sparta::work_queue<const Method*>([&](const Method* method) {
        const auto& callees = context.call_graph->callees(method);
        for (const auto& call_target : callees) {
          if (register_receiver_methods.find(
                  call_target.resolved_base_callee()) ==
              register_receiver_methods.end()) {
            continue;
          }

          const auto* broadcast_receiver_type = context.types->source_type(
              method,
              call_target.instruction(),
              k_broadcast_receiver_source_position);
          if (broadcast_receiver_type == nullptr) {
            continue;
          }

          // Make sure that the broadcast_receiver_type is not one of the base
          // classes and that it inherits from at least one of the base classes
          if (k_broadcast_receiver_base_classes.find(
                  broadcast_receiver_type->str()) !=
              k_broadcast_receiver_base_classes.end()) {
            continue;
          }
          auto* klass = type_class(broadcast_receiver_type);
          if (klass == nullptr) {
            continue;
          }
          const auto& parents = generator::get_parents_from_class(
              klass, /* include_interfaces */ false);
          for (const auto& base_class_type :
               k_broadcast_receiver_base_classes) {
            if (parents.find(base_class_type) != parents.end()) {
              broadcast_receiver_types.emplace(broadcast_receiver_type);
              break;
            }
          }
        }
      });
  for (const auto* method : methods) {
    get_caller_queue.add_item(method);
  }
  get_caller_queue.run_all();
  return broadcast_receiver_types;
}

} // namespace

std::vector<Model> BroadcastReceiverGenerator::run(const Methods& methods) {
  ConcurrentSet<const Method*> register_receiver_methods;
  auto get_register_receiver_queue =
      sparta::work_queue<const Method*>([&](const Method* method) {
        if (method->get_name() == "registerReceiver" &&
            method->number_of_parameters() ==
                k_register_receiver_number_arguments) {
          if (k_ignored_register_receiver_signatures.find(
                  method->signature()) ==
              k_ignored_register_receiver_signatures.end()) {
            register_receiver_methods.emplace(method);
          }
        }
      });
  for (const auto* method : methods) {
    get_register_receiver_queue.add_item(method);
  }
  get_register_receiver_queue.run_all();

  const auto& broadcast_receiver_types = get_broadcast_receiver_types(
      context_, methods, register_receiver_methods);

  std::vector<Model> models;
  for (const auto* broadcast_receiver_type : broadcast_receiver_types) {
    const auto* type = redex::get_type(broadcast_receiver_type->str());
    if (!type) {
      continue;
    }
    const auto* klass = type_class(type);
    if (!klass) {
      continue;
    }
    for (const auto* dex_method : klass->get_all_methods()) {
      if (dex_method->get_name()->str() == "onReceive") {
        const auto* method = context_.methods->get(dex_method);
        auto model = Model(method, context_);
        model.add_parameter_source(
            AccessPath(Root(
                Root::Kind::Argument, k_on_receive_intent_argument_position)),
            generator::source(
                context_,
                method,
                /* kind */ "ReceiverUserInput",
                {"via-caller-exported"}));
        model.add_mode(Model::Mode::NoJoinVirtualOverrides, context_);
        models.push_back(model);
      }
    }
  }

  return models;
}

} // namespace marianatrench
