/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <sparta/MonotonicFixpointIterator.h>

#include <ControlFlow.h>
#include <InstructionAnalyzer.h>
#include <Show.h>

#include <mariana-trench/CallGraph.h>
#include <mariana-trench/Constants.h>
#include <mariana-trench/IntentRoutingAnalyzer.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

namespace {

using RoutedIntents = PatriciaTreeSetAbstractDomain<
    const DexType*,
    /* bottom_is_empty */ true,
    /* with_top */ true>;

using InstructionsToRoutedIntents = sparta::
    PatriciaTreeMapAbstractPartition<const IRInstruction*, RoutedIntents>;

class IntentRoutingContext final {
 public:
  IntentRoutingContext(
      ReceivingMethod& method,
      const Types& types,
      const Options& options)
      : routed_intents_({}),
        receiving_method_(method),
        types_(types),
        dump_(receiving_method_.method->should_be_logged(options)) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(IntentRoutingContext)

  void add_routed_intent(
      const IRInstruction* instruction,
      const DexType* type) {
    auto routed_intents = routed_intents_.get(instruction);
    routed_intents.add(type);
    routed_intents_.set(instruction, routed_intents);
  }

  void mark_as_getting_routed_intent(Root root, Component component) {
    receiving_method_.root = root;
    receiving_method_.component = component;
  }

  ReceivingMethod method_gets_routed_intent() {
    return receiving_method_;
  }

  const InstructionsToRoutedIntents& instructions_to_routed_intents() {
    return routed_intents_;
  }

  const Method* method() {
    return receiving_method_.method;
  }

  const Types& types() {
    return types_;
  }

  bool dump() const {
    return dump_;
  }

 private:
  InstructionsToRoutedIntents routed_intents_;
  ReceivingMethod receiving_method_;
  const Types& types_;
  bool dump_;
};

class Transfer final : public InstructionAnalyzerBase<
                           Transfer,
                           InstructionsToRoutedIntents,
                           IntentRoutingContext*> {
 public:
  static bool analyze_default(
      IntentRoutingContext* /* context */,
      const IRInstruction* /* instruction */,
      InstructionsToRoutedIntents* /* current_state */) {
    return false;
  }

  static bool analyze_invoke(
      IntentRoutingContext* context,
      const IRInstruction* instruction,
      InstructionsToRoutedIntents* /* current_state */) {
    LOG_OR_DUMP(context, 4, "Analyzing instruction: {}", show(instruction));
    DexMethodRef* dex_method_reference = instruction->get_method();
    auto method = resolve_method_deprecated(
        dex_method_reference,
        opcode_to_search(instruction->opcode()),
        context->method()->dex_method());

    if (method == nullptr || method->get_class() == nullptr) {
      return false;
    }

    const auto& intent_class_setters = constants::get_intent_class_setters();
    auto intent_parameter_position = intent_class_setters.find(show(method));
    if (intent_parameter_position != intent_class_setters.end()) {
      auto class_index = intent_parameter_position->second;

      auto argument_index = class_index;
      if (!::is_static(method)) {
        // For instance methods, class_index includes implicit `this` at
        // position 0. Here, we want the index in the method proto's argument
        // list, which does _not_ include the `this` argument.
        mt_assert(class_index > 0);
        argument_index = class_index - 1;
      }

      const auto dex_arguments = method->get_proto()->get_args();
      mt_assert(argument_index < dex_arguments->size());
      if (dex_arguments->at(argument_index) != type::java_lang_Class()) {
        return false;
      }
      const auto& environment = context->types().const_class_environment(
          context->method(), instruction);
      mt_assert(class_index < instruction->srcs_size());
      auto found = environment.find(instruction->src(class_index));
      if (found == environment.end()) {
        return false;
      }

      const auto* type = found->second.singleton_type();
      LOG_OR_DUMP(
          context,
          4,
          "Method `{}` routes Intent to `{}`",
          context->method()->show(),
          show(type));
      context->add_routed_intent(instruction, type);
    } else if (
        method->get_name()->str() == "getIntent" &&
        method->get_proto()->get_rtype()->str() == "Landroid/content/Intent;") {
      LOG_OR_DUMP(
          context,
          4,
          "Method `{}` calls getIntent()",
          context->method()->show());
      context->mark_as_getting_routed_intent(
          Root(Root::Kind::CallEffectIntent), Component::Activity);
    }
    return false;
  }
};

class IntentRoutingFixpointIterator final
    : public sparta::MonotonicFixpointIterator<
          cfg::GraphInterface,
          InstructionsToRoutedIntents> {
 public:
  IntentRoutingFixpointIterator(
      const cfg::ControlFlowGraph& cfg,
      InstructionAnalyzer<InstructionsToRoutedIntents> instruction_analyzer)
      : MonotonicFixpointIterator(cfg),
        instruction_analyzer_(instruction_analyzer) {}

  void analyze_node(
      const NodeId& block,
      InstructionsToRoutedIntents* current_state) const override {
    for (auto& entry : *block) {
      switch (entry.type) {
        case MFLOW_OPCODE:
          instruction_analyzer_(entry.insn, current_state);
          break;

        default:
          break;
      }
    }
  }

  InstructionsToRoutedIntents analyze_edge(
      const cfg::GraphInterface::EdgeId&,
      const InstructionsToRoutedIntents& exit_state) const override {
    return exit_state;
  }

 private:
  InstructionAnalyzer<InstructionsToRoutedIntents> instruction_analyzer_;
};

struct IntentRoutingData {
 public:
  ReceivingMethod receiving_intent_root;
  std::vector<const DexType*> routed_intents;
};

std::pair<std::optional<Component>, std::optional<ShimRoot>>
get_position_from_callee(const Method* original_callee) {
  const auto& activity_routing_methods =
      constants::get_activity_routing_methods();
  const auto& service_routing_methods =
      constants::get_service_routing_methods();

  auto activity_match =
      activity_routing_methods.find(original_callee->signature());
  if (activity_match != activity_routing_methods.end()) {
    return std::pair(
        Component::Activity, Root::argument(activity_match->second));
  }
  auto service_match =
      service_routing_methods.find(original_callee->signature());
  if (service_match != service_routing_methods.end()) {
    return std::pair(Component::Service, Root::argument(service_match->second));
  }

  auto receiver_methods =
      constants::get_broadcast_receiver_routing_method_names();
  auto receiver_match =
      receiver_methods.find(std::string(original_callee->get_name()));
  if (receiver_match != receiver_methods.end()) {
    auto args = original_callee->get_proto()->get_args();
    if (args->size() > 0 &&
        args->at(0) == DexType::make_type("Landroid/content/Intent;")) {
      return std::pair(
          Component::BroadcastReceiver,
          Root::argument(::is_static(original_callee) ? 0 : 1));
    }
  }

  return std::pair(std::nullopt, std::nullopt);
}

IntentRoutingData method_routes_intents_to(
    const Method* method,
    const Types& types,
    const Options& options) {
  ReceivingMethod receiving_method = {
      .method = method,
  };
  auto* code = method->get_code();
  if (code == nullptr) {
    return {/* receiving_intent_root */ receiving_method,
            /* routed_intents */ {}};
  }

  if (!code->cfg_built()) {
    LOG(1,
        "CFG not built for method: {}. Cannot evaluate routed intents.",
        method->show());
    return {/* receiving_intent_root */ receiving_method,
            /* routed_intents */ {}};
  }

  auto context =
      std::make_unique<IntentRoutingContext>(receiving_method, types, options);

  auto intent_receiving_method_names =
      constants::get_intent_receiving_method_names();
  auto match =
      intent_receiving_method_names.find(std::string(method->get_name()));
  if (match != intent_receiving_method_names.end()) {
    auto args = method->get_proto()->get_args();
    if (args->size() >= match->second.first &&
        args->at(match->second.first - 1) ==
            DexType::get_type("Landroid/content/Intent;")) {
      context->mark_as_getting_routed_intent(
          Root(Root::Kind::Argument, match->second.first),
          match->second.second);
    }
  }

  auto fixpoint = IntentRoutingFixpointIterator(
      code->cfg(), InstructionAnalyzerCombiner<Transfer>(context.get()));
  InstructionsToRoutedIntents instructions_to_routed_intents{};
  fixpoint.run(instructions_to_routed_intents);
  std::vector<const DexType*> routed_intents;
  for (auto& instruction_to_intents :
       context->instructions_to_routed_intents().bindings()) {
    for (auto intent : instruction_to_intents.second.elements()) {
      routed_intents.push_back(intent);
    }
  }
  return IntentRoutingData{
      /* receiving_intent_root */ context->method_gets_routed_intent(),
      routed_intents};
}

} // namespace

std::unique_ptr<IntentRoutingAnalyzer> IntentRoutingAnalyzer::run(
    const Methods& methods,
    const Types& types,
    const Options& options) {
  auto analyzer = std::make_unique<IntentRoutingAnalyzer>();

  auto queue = sparta::work_queue<const Method*>([&](const Method* method) {
    auto intent_routing_data = method_routes_intents_to(method, types, options);
    if (intent_routing_data.receiving_intent_root.root != std::nullopt) {
      LOG(5,
          "Shimming {} as a method that receives an Intent.",
          method->show());
      auto klass = method->get_class();
      analyzer->classes_to_intent_receivers_.update(
          klass,
          [&intent_routing_data](
              const DexType* /* key */,
              std::vector<ReceivingMethod>& methods,
              bool /* exists */) {
            methods.emplace_back(intent_routing_data.receiving_intent_root);
            return methods;
          });
    }

    if (!intent_routing_data.routed_intents.empty()) {
      LOG(5,
          "Shimming {} as a method that routes intents cross-component.",
          method->show());
      analyzer->methods_to_routed_intents_.emplace(
          method, intent_routing_data.routed_intents);
    }
  });

  for (const auto* method : methods) {
    queue.add_item(method);
  }
  queue.run_all();

  return analyzer;
}

InstantiatedShim::FlatSet<ShimTarget>
IntentRoutingAnalyzer::get_intent_routing_targets(
    const Method* original_callee,
    const Method* caller) const {
  InstantiatedShim::FlatSet<ShimTarget> intent_routing_targets;

  // Check if callee is an intent launcher
  auto [component, position] = get_position_from_callee(original_callee);
  if (!component || !position) {
    return intent_routing_targets;
  }

  // check if the method handles incoming intents (e.g. getIntent())
  auto routed_intent_classes = methods_to_routed_intents_.find(caller);
  if (routed_intent_classes == methods_to_routed_intents_.end()) {
    return intent_routing_targets;
  }

  for (const auto& intent_class : routed_intent_classes->second) {
    auto intent_receivers = classes_to_intent_receivers_.find(intent_class);
    if (intent_receivers == classes_to_intent_receivers_.end()) {
      continue;
    }
    for (const auto& intent_receiver : intent_receivers->second) {
      if (*component != intent_receiver.component ||
          intent_receiver.root == std::nullopt) {
        continue;
      }
      intent_routing_targets.emplace(
          intent_receiver.method,
          ShimParameterMapping({{*intent_receiver.root, *position}}));
    }
  }

  return intent_routing_targets;
}

} // namespace marianatrench
