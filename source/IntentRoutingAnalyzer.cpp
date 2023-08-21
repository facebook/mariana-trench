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

static std::unordered_map<std::string, ParameterPosition> intent_class_setters =
    constants::get_intent_class_setters();

using RoutedIntents = PatriciaTreeSetAbstractDomain<
    const DexType*,
    /* bottom_is_empty */ true,
    /* with_top */ true>;

using InstructionsToRoutedIntents = sparta::
    PatriciaTreeMapAbstractPartition<const IRInstruction*, RoutedIntents>;

class IntentRoutingContext final {
 public:
  IntentRoutingContext(
      const Method* method,
      const Types& types,
      const Options& options)
      : routed_intents_({}),
        method_(method),
        types_(types),
        routed_intent_root_(),
        dump_(method->should_be_logged(options)) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(IntentRoutingContext)

  void add_routed_intent(
      const IRInstruction* instruction,
      const DexType* type) {
    auto routed_intents = routed_intents_.get(instruction);
    routed_intents.add(type);
    routed_intents_.set(instruction, routed_intents);
  }

  void mark_as_getting_routed_intent(Root root) {
    routed_intent_root_ = root;
  }

  std::optional<Root> method_gets_routed_intent() {
    return routed_intent_root_;
  }

  const InstructionsToRoutedIntents& instructions_to_routed_intents() {
    return routed_intents_;
  }

  const Method* method() {
    return method_;
  }

  const Types& types() {
    return types_;
  }

  bool dump() const {
    return dump_;
  }

 private:
  InstructionsToRoutedIntents routed_intents_;
  const Method* method_;
  const Types& types_;
  std::optional<Root> routed_intent_root_;
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
    auto method = resolve_method(
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

      mt_assert(class_index > 0);
      mt_assert(!::is_static(method));

      const auto dex_arguments = method->get_proto()->get_args();
      if (dex_arguments->at(class_index - 1) != type::java_lang_Class()) {
        return false;
      }
      const auto& environment = context->types().const_class_environment(
          context->method(), instruction);
      mt_assert(class_index < instruction->srcs_size());
      auto found = environment.find(instruction->src(class_index));
      if (found == environment.end()) {
        return false;
      }

      const auto* type = found->second;
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
          Root(Root::Kind::CallEffectIntent));
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
  std::optional<Root> receiving_intent_root;
  std::vector<const DexType*> routed_intents;
};

IntentRoutingData method_routes_intents_to(
    const Method* method,
    const Types& types,
    const Options& options) {
  auto* code = method->get_code();
  if (code == nullptr) {
    return {/* receiving_intent_root */ std::nullopt, /* routed_intents */ {}};
  }

  if (!code->cfg_built()) {
    LOG(1,
        "CFG not built for method: {}. Cannot evaluate routed intents.",
        method->show());
    return {/* receiving_intent_root */ std::nullopt, /* routed_intents */ {}};
  }

  auto context = std::make_unique<IntentRoutingContext>(method, types, options);

  auto intent_receiving_method_names =
      constants::get_intent_receiving_method_names();
  auto match =
      intent_receiving_method_names.find(std::string(method->get_name()));
  if (match != intent_receiving_method_names.end()) {
    auto args = method->get_proto()->get_args();
    if (args->size() >= match->second &&
        args->at(match->second - 1) ==
            DexType::get_type("Landroid/content/Intent;")) {
      context->mark_as_getting_routed_intent(
          Root(Root::Kind::Argument, match->second));
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
  return {
      /* receiving_intent_root */ context->method_gets_routed_intent(),
      routed_intents};
}

static std::unordered_map<std::string, ParameterPosition>
    activity_routing_methods = constants::get_activity_routing_methods();
static std::unordered_map<std::string, ParameterPosition>
    service_routing_methods = constants::get_service_routing_methods();

} // namespace

IntentRoutingAnalyzer IntentRoutingAnalyzer::run(const Context& context) {
  if (!context.options->enable_cross_component_analysis()) {
    return IntentRoutingAnalyzer();
  }

  IntentRoutingAnalyzer analyzer;
  auto queue = sparta::work_queue<const Method*>([&](const Method* method) {
    auto intent_routing_data =
        method_routes_intents_to(method, *context.types, *context.options);
    if (intent_routing_data.receiving_intent_root != std::nullopt) {
      LOG(5,
          "Shimming {} as a method that receivings an Intent.",
          method->show());
      auto klass = method->get_class();
      auto root = *intent_routing_data.receiving_intent_root;
      analyzer.classes_to_intent_receivers_.update(
          klass,
          [&method, &root](
              const DexType* /* key */,
              std::vector<std::pair<const Method*, Root>>& methods,
              bool /* exists */) {
            methods.emplace_back(method, root);
            return methods;
          });
    }

    if (!intent_routing_data.routed_intents.empty()) {
      LOG(5,
          "Shimming {} as a method that routes intents cross-component.",
          method->show());
      analyzer.methods_to_routed_intents_.emplace(
          method, intent_routing_data.routed_intents);
    }
  });

  auto* methods = context.methods.get();
  for (auto iterator = methods->begin(); iterator != methods->end();
       ++iterator) {
    queue.add_item(*iterator);
  }
  queue.run_all();

  return analyzer;
}

std::vector<ShimTarget> IntentRoutingAnalyzer::get_intent_routing_targets(
    const Method* original_callee,
    const Method* caller) const {
  std::vector<ShimTarget> intent_routing_targets;

  // Find if the callee is an Activity launcher (e.g. startActivity)
  auto intent_parameter_position =
      activity_routing_methods.find(original_callee->signature());
  if (intent_parameter_position == activity_routing_methods.end()) {
    // Find if the callee is an Service launcher (e.g. startService)
    intent_parameter_position =
        service_routing_methods.find(original_callee->signature());
    if (intent_parameter_position == service_routing_methods.end()) {
      return intent_routing_targets;
    }
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
      intent_routing_targets.emplace_back(
          intent_receiver.first,
          ShimParameterMapping(
              {{intent_receiver.second, intent_parameter_position->second}}));
    }
  }

  return intent_routing_targets;
}

} // namespace marianatrench
