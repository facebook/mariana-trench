/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <ControlFlow.h>
#include <HashedAbstractEnvironment.h>
#include <HashedSetAbstractDomain.h>
#include <InstructionAnalyzer.h>
#include <MonotonicFixpointIterator.h>
#include <Show.h>

#include <mariana-trench/CallGraph.h>
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
  IntentRoutingContext(const Method* method, const Types& types)
      : routed_intents_({}), method_(method), types_(types) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(IntentRoutingContext)

  void add_routed_intent(
      const IRInstruction* instruction,
      const DexType* type) {
    auto routed_intents = routed_intents_.get(instruction);
    routed_intents.add(type);
    routed_intents_.set(instruction, routed_intents);
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

 private:
  InstructionsToRoutedIntents routed_intents_;
  const Method* method_;
  const Types& types_;
};

class Transfer final : public InstructionAnalyzerBase<
                           Transfer,
                           InstructionsToRoutedIntents,
                           IntentRoutingContext*> {
 public:
  static bool analyze_default(
      IntentRoutingContext* context,
      const IRInstruction* instruction,
      InstructionsToRoutedIntents* current_state) {
    LOG(4, "Analyzing instruction: {}", show(instruction));
    return false;
  }

  static bool analyze_invoke(
      IntentRoutingContext* context,
      const IRInstruction* instruction,
      InstructionsToRoutedIntents* current_state) {
    LOG(4, "Analyzing instruction: {}", show(instruction));
    if (instruction->opcode() != OPCODE_INVOKE_DIRECT) {
      return false;
    }
    DexMethodRef* dex_method_reference = instruction->get_method();
    DexMethod* method = resolve_method(
        dex_method_reference,
        opcode_to_search(instruction->opcode()),
        context->method()->dex_method());
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
      context->add_routed_intent(instruction, type);
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

} // namespace

std::vector<const DexType*> method_routes_intents_to(
    const Method* method,
    const Types& types) {
  auto* code = method->get_code();
  if (code == nullptr) {
    return {};
  }

  if (!code->cfg_built()) {
    LOG(1,
        "CFG not built for method: {}. Cannot evaluate routed intents.",
        method->show());
    return {};
  }

  auto context = std::make_unique<IntentRoutingContext>(method, types);
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
  return routed_intents;
}

} // namespace marianatrench
