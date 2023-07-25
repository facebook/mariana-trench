/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <ControlFlow.h>
#include <InstructionAnalyzer.h>
#include <MonotonicFixpointIterator.h>
#include <Show.h>

#include <mariana-trench/CallGraph.h>
#include <mariana-trench/Constants.h>
#include <mariana-trench/IntentRoutingAnalyzer.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

namespace {

const std::string ANDROID_INTENT_CLASS = "Landroid/content/Intent;";

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
        gets_routed_intent_(false),
        dump_(method->should_be_logged(options)) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(IntentRoutingContext)

  void add_routed_intent(
      const IRInstruction* instruction,
      const DexType* type) {
    auto routed_intents = routed_intents_.get(instruction);
    routed_intents.add(type);
    routed_intents_.set(instruction, routed_intents);
  }

  void mark_as_getting_routed_intent() {
    gets_routed_intent_ = true;
  }

  bool method_gets_routed_intent() {
    return gets_routed_intent_;
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
  bool gets_routed_intent_;
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
    // Handle new Intent(context, C.class) and intent.setClass(context,
    // C.class).
    if (method->get_class()->get_name()->str() == ANDROID_INTENT_CLASS &&
        (method->get_name()->str() == "<init>" ||
         method->get_name()->str() == "setClass")) {
      const auto dex_arguments = method->get_proto()->get_args();
      if (dex_arguments->size() != 2) {
        return false;
      }
      const std::size_t class_index = 1;
      if (dex_arguments->at(class_index) != type::java_lang_Class()) {
        return false;
      }
      const auto& environment = context->types().const_class_environment(
          context->method(), instruction);
      auto found = environment.find(instruction->src(class_index + 1));
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
      context->mark_as_getting_routed_intent();
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
  bool calls_get_intent;
  std::vector<const DexType*> routed_intents;
};

IntentRoutingData method_routes_intents_to(
    const Method* method,
    const Types& types,
    const Options& options) {
  auto* code = method->get_code();
  if (code == nullptr) {
    return {/* calls_get_intent */ false, /* routed_intents */ {}};
  }

  if (!code->cfg_built()) {
    LOG(1,
        "CFG not built for method: {}. Cannot evaluate routed intents.",
        method->show());
    return {/* calls_get_intent */ false, /* routed_intents */ {}};
  }

  auto context = std::make_unique<IntentRoutingContext>(method, types, options);
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
      /* calls_get_intent */ context->method_gets_routed_intent(),
      routed_intents};
}

static std::unordered_map<std::string, ParameterPosition>
    activity_routing_methods = constants::get_activity_routing_methods();

} // namespace

IntentRoutingAnalyzer IntentRoutingAnalyzer::run(const Context& context) {
  if (!context.options->enable_cross_component_analysis()) {
    return IntentRoutingAnalyzer();
  }

  IntentRoutingAnalyzer analyzer;
  auto queue = sparta::work_queue<const Method*>([&](const Method* method) {
    auto intent_routing_data =
        method_routes_intents_to(method, *context.types, *context.options);
    if (intent_routing_data.calls_get_intent) {
      LOG(5, "Shimming {} as a method that calls getIntent().", method->show());
      auto klass = method->get_class();
      analyzer.classes_to_intent_getters_.update(
          klass,
          [&method](
              const DexType* /* key */,
              std::vector<const Method*>& methods,
              bool /* exists */) {
            methods.emplace_back(method);
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

  // Intent routing does not exist unless the callee is in the vein of
  // an activity routing method (e.g. startActivity).
  auto intent_parameter_position =
      activity_routing_methods.find(original_callee->signature());
  if (intent_parameter_position == activity_routing_methods.end()) {
    return intent_routing_targets;
  }

  auto routed_intent_classes = methods_to_routed_intents_.find(caller);
  if (routed_intent_classes == methods_to_routed_intents_.end()) {
    return intent_routing_targets;
  }

  for (const auto& intent_class : routed_intent_classes->second) {
    auto intent_getters = classes_to_intent_getters_.find(intent_class);
    if (intent_getters == classes_to_intent_getters_.end()) {
      continue;
    }
    for (const auto& intent_getter : intent_getters->second) {
      intent_routing_targets.emplace_back(
          intent_getter,
          ShimParameterMapping(
              {{Root(Root::Kind::CallEffectIntent),
                intent_parameter_position->second}}));
    }
  }

  return intent_routing_targets;
}

} // namespace marianatrench
