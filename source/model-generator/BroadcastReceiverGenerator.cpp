/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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

// The source (argument) number of the BroadcastReceiver argument in the
// IRInstruction invoking the registerReceiver method
const std::size_t k_broadcast_receiver_source_position = 1;

// The argument position (including implicit this) of the intent argument in
// BroadcastReceiver.onReceive()
const marianatrench::ParameterPosition k_on_receive_intent_argument_position =
    2;

const std::unordered_set<std::string> k_exported_register_signatures = {
    "Landroid/content/Context;.registerReceiver:(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)",
    "Landroid/content/Context;.registerReceiver:(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;I)",
    "Landroid/content/ContextWrapper;.registerReceiver:(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)",
    "Landroid/content/ContextWrapper;.registerReceiver:(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;I)",
};

ConcurrentSet<const DexClass*> get_broadcast_receiver_types(
    const Context& context,
    const Methods& methods,
    const ConcurrentSet<const Method*>& register_receiver_methods) {
  ConcurrentSet<const DexClass*> broadcast_receiver_types;
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

          const auto* type = redex::get_type(broadcast_receiver_type->str());
          if (!type) {
            continue;
          }
          const auto* klass = type_class(type);
          if (!klass) {
            continue;
          }

          broadcast_receiver_types.emplace(klass);
        }
      });
  for (const auto* method : methods) {
    get_caller_queue.add_item(method);
  }
  get_caller_queue.run_all();
  return broadcast_receiver_types;
}

} // namespace

std::vector<Model> BroadcastReceiverGenerator::emit_method_models(
    const Methods& methods) {
  ConcurrentSet<const Method*> exported_register_methods;
  auto get_register_receiver_queue =
      sparta::work_queue<const Method*>([&](const Method* method) {
        const auto method_signature = show(method);
        for (const auto& signature : k_exported_register_signatures) {
          if (boost::starts_with(method_signature, signature)) {
            exported_register_methods.emplace(method);
          }
        }
      });

  for (const auto* method : methods) {
    get_register_receiver_queue.add_item(method);
  }
  get_register_receiver_queue.run_all();

  const auto& exported_receiver_types = get_broadcast_receiver_types(
      context_, methods, exported_register_methods);

  std::vector<Model> models;
  for (const auto* broadcast_receiver_type :
       UnorderedIterable(exported_receiver_types)) {
    for (const auto* dex_method : broadcast_receiver_type->get_all_methods()) {
      if (dex_method->get_name()->str() == "onReceive") {
        const auto* method = context_.methods->get(dex_method);
        auto model = Model(method, context_);
        model.add_parameter_source(
            AccessPath(Root(
                Root::Kind::Argument, k_on_receive_intent_argument_position)),
            generator::source(
                context_,
                /* kind */ "ReceiverUserInput",
                {"via-caller-exported", "via-dynamic-receiver"}),
            *context_.heuristics);
        model.add_call_effect_source(
            AccessPath(Root(Root::Kind::CallEffectExploitability)),
            generator::source(
                context_,
                /* kind */ "ExportedComponent"),
            *context_.heuristics);
        model.add_mode(Model::Mode::NoJoinVirtualOverrides, context_);
        model.add_model_generator(name_);
        models.push_back(model);
      }
    }
  }

  return models;
}

} // namespace marianatrench
