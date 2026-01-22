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

using SendTargetsDomain = PatriciaTreeSetAbstractDomain<
    const DexType*,
    /* bottom_is_empty */ true,
    /* with_top */ true>;

using SendPointToSendTargetsDomain = sparta::
    PatriciaTreeMapAbstractPartition<const IRInstruction*, SendTargetsDomain>;

class IntentRoutingContext final {
 public:
  IntentRoutingContext(
      IntentRoutingAnalyzer::ReceivePoint& method,
      const Types& types,
      const Options& options)
      : send_point_to_send_targets_({}),
        receiving_point_(method),
        types_(types),
        dump_(receiving_point_.method->should_be_logged(options)) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(IntentRoutingContext)

  void add_send_target(const IRInstruction* instruction, const DexType* type) {
    auto send_targets = send_point_to_send_targets_.get(instruction);
    send_targets.add(type);
    send_point_to_send_targets_.set(instruction, send_targets);
  }

  void mark_as_receive_point(Root root, Component component) {
    receiving_point_.root = root;
    receiving_point_.component = component;
  }

  IntentRoutingAnalyzer::ReceivePoint receive_point() {
    return receiving_point_;
  }

  const SendPointToSendTargetsDomain& send_points_to_send_targets() {
    return send_point_to_send_targets_;
  }

  const Method* method() {
    return receiving_point_.method;
  }

  const Types& types() {
    return types_;
  }

  bool dump() const {
    return dump_;
  }

 private:
  SendPointToSendTargetsDomain send_point_to_send_targets_;
  IntentRoutingAnalyzer::ReceivePoint receiving_point_;
  const Types& types_;
  bool dump_;
};

class Transfer final : public InstructionAnalyzerBase<
                           Transfer,
                           SendPointToSendTargetsDomain,
                           IntentRoutingContext*> {
 public:
  static bool analyze_default(
      IntentRoutingContext* /* context */,
      const IRInstruction* /* instruction */,
      SendPointToSendTargetsDomain* /* current_state */) {
    return false;
  }

  static bool analyze_invoke(
      IntentRoutingContext* context,
      const IRInstruction* instruction,
      SendPointToSendTargetsDomain* /* current_state */) {
    LOG_OR_DUMP(context, 4, "Analyzing instruction: {}", show(instruction));
    DexMethodRef* dex_method_reference = instruction->get_method();
    auto method = resolve_method_deprecated(
        dex_method_reference,
        opcode_to_search(instruction->opcode()),
        context->method()->dex_method());

    if (method == nullptr || method->get_class() == nullptr) {
      return false;
    }

    auto method_signature = show(method);

    // Check for reflection-based-based (java.lang.Class or java.lang.String)
    // intent class setters. E.g., Intent(context, MyActivity.class),
    // Intent(context, "MyActivity")
    const auto& intent_class_setters = constants::get_intent_class_setters();
    auto intent_parameter_position =
        intent_class_setters.find(method_signature);
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
      const auto* argument = dex_arguments->at(argument_index);

      if (argument != type::java_lang_Class() &&
          argument != type::java_lang_String()) {
        return false;
      }

      const auto* type = context->types().register_reflected_type(
          context->method(), instruction, instruction->src(class_index));

      if (type == nullptr) {
        return false;
      }

      LOG_OR_DUMP(
          context,
          4,
          "Method `{}` routes Intent to `{}`",
          context->method()->show(),
          show(type));

      context->add_send_target(instruction, type);
    } else if (
        method->get_name()->str() ==
            constants::get_intent_receiving_api_method_name() &&
        method->get_proto()->get_rtype()->str() == "Landroid/content/Intent;") {
      LOG_OR_DUMP(
          context,
          4,
          "Method `{}` calls receive-api `{}`",
          context->method()->show(),
          method_signature);
      context->mark_as_receive_point(
          Root(Root::Kind::CallEffectIntent), Component::Activity);
    }

    return false;
  }
};

class IntentRoutingFixpointIterator final
    : public sparta::MonotonicFixpointIterator<
          cfg::GraphInterface,
          SendPointToSendTargetsDomain> {
 public:
  IntentRoutingFixpointIterator(
      const cfg::ControlFlowGraph& cfg,
      InstructionAnalyzer<SendPointToSendTargetsDomain> instruction_analyzer)
      : MonotonicFixpointIterator(cfg),
        instruction_analyzer_(instruction_analyzer) {}

  void analyze_node(
      const NodeId& block,
      SendPointToSendTargetsDomain* current_state) const override {
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

  SendPointToSendTargetsDomain analyze_edge(
      const cfg::GraphInterface::EdgeId&,
      const SendPointToSendTargetsDomain& exit_state) const override {
    return exit_state;
  }

 private:
  InstructionAnalyzer<SendPointToSendTargetsDomain> instruction_analyzer_;
};

struct IntentRoutingData {
 public:
  IntentRoutingAnalyzer::ReceivePoint receive_point;
  IntentRoutingAnalyzer::SendTargets send_targets;
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
  IntentRoutingAnalyzer::ReceivePoint receive_point = {
      .method = method,
      .root = std::nullopt,
      .component = std::nullopt,
  };
  auto* code = method->get_code();
  if (code == nullptr) {
    return {/* receive_point */ receive_point,
            /* send_targets */ {}};
  }

  if (!code->cfg_built()) {
    LOG(1,
        "CFG not built for method: {}. Cannot evaluate routed intents.",
        method->show());
    return {/* receive_point */ receive_point,
            /* send_targets */ {}};
  }

  auto context =
      std::make_unique<IntentRoutingContext>(receive_point, types, options);

  auto intent_receiving_method_names =
      constants::get_intent_receiving_method_names();
  auto match =
      intent_receiving_method_names.find(std::string(method->get_name()));
  if (match != intent_receiving_method_names.end()) {
    auto args = method->get_proto()->get_args();
    if (args->size() >= match->second.first &&
        args->at(match->second.first - 1) ==
            DexType::get_type("Landroid/content/Intent;")) {
      context->mark_as_receive_point(
          Root(Root::Kind::Argument, match->second.first),
          match->second.second);
    }
  }

  auto fixpoint = IntentRoutingFixpointIterator(
      code->cfg(), InstructionAnalyzerCombiner<Transfer>(context.get()));
  SendPointToSendTargetsDomain send_points_to_send_targets{};
  fixpoint.run(send_points_to_send_targets);
  IntentRoutingAnalyzer::SendTargets send_targets;
  for (auto& instruction_to_intents :
       context->send_points_to_send_targets().bindings()) {
    for (auto intent : instruction_to_intents.second.elements()) {
      send_targets.push_back(intent);
    }
  }
  return IntentRoutingData{/* receive_point */ context->receive_point(),
                           send_targets};
}

} // namespace

std::unique_ptr<IntentRoutingAnalyzer> IntentRoutingAnalyzer::run(
    const Methods& methods,
    const Types& types,
    const Options& options) {
  auto analyzer = std::make_unique<IntentRoutingAnalyzer>();

  auto queue = sparta::work_queue<const Method*>([&](const Method* method) {
    auto intent_routing_data = method_routes_intents_to(method, types, options);

    // Process receive-points: Methods that receive intents from other
    // components
    if (intent_routing_data.receive_point.root != std::nullopt) {
      LOG(5,
          "Shimming {} as a method that receives an Intent (receive-point).",
          method->show());
      auto klass = method->get_class();
      analyzer->target_classes_to_receive_points_.update(
          klass,
          [&intent_routing_data](
              const DexType* /* key */,
              std::vector<ReceivePoint>& methods,
              bool /* exists */) {
            methods.emplace_back(intent_routing_data.receive_point);
            return methods;
          });
    }

    // Process send-points: Methods that route intents to other components
    if (!intent_routing_data.send_targets.empty()) {
      LOG(5,
          "Shimming {} as a method that routes intents cross-component (send-point).",
          method->show());
      analyzer->method_to_send_targets_.emplace(
          method, intent_routing_data.send_targets);
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

  // Check if callee is a send-point (intent launcher API like startActivity)
  auto [component, position] = get_position_from_callee(original_callee);
  if (!component || !position) {
    return intent_routing_targets;
  }

  // Check if the caller is a method that sets target classes for intents
  auto send_targets_entry = method_to_send_targets_.find(caller);
  if (send_targets_entry == method_to_send_targets_.end()) {
    return intent_routing_targets;
  }

  // For each target class, find all receive-points that can handle the intent
  for (const auto& target_class : send_targets_entry->second) {
    auto receive_points_entry =
        target_classes_to_receive_points_.find(target_class);
    if (receive_points_entry == target_classes_to_receive_points_.end()) {
      continue;
    }

    // For each receive-point in the target class
    for (const auto& receive_point : receive_points_entry->second) {
      // Skip if component type doesn't match or if the receive-point doesn't
      // have a root
      if (*component != receive_point.component ||
          receive_point.root == std::nullopt) {
        continue;
      }

      // Create a parameter mapping to connect the send-point to the
      // receive-point
      intent_routing_targets.emplace(
          receive_point.method,
          ShimParameterMapping({{*receive_point.root, *position}}));
    }
  }

  return intent_routing_targets;
}

} // namespace marianatrench
