/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/ForwardAliasFixpoint.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/TransferCall.h>

namespace marianatrench {

ForwardAliasFixpoint::ForwardAliasFixpoint(
    MethodContext& context,
    const cfg::ControlFlowGraph& cfg,
    InstructionAnalyzer<ForwardAliasEnvironment> instruction_analyzer)
    : MonotonicFixpointIterator(cfg, cfg.num_blocks()),
      context_(context),
      instruction_analyzer_(std::move(instruction_analyzer)) {}

ForwardAliasFixpoint::~ForwardAliasFixpoint() {}

namespace {

// We could just use a switch on the opcode, but we use `InstructionAnalyzer`
// since it's also used in the forward and backward transfer function, hence we
// know for sure that we are consistent.
class ShouldStoreAliasResults final
    : public InstructionAnalyzerBase<ShouldStoreAliasResults, bool> {
 public:
  static bool analyze_default(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = false;
    return false;
  }

  static bool analyze_check_cast(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_iget(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_sget(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_invoke(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_iput(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_sput(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_load_param(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_aput(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_new_array(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_filled_new_array(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_unop(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_binop(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_binop_lit(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_return(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }

  static bool analyze_const_string(
      const IRInstruction* /* instruction */,
      bool* should_store_alias_results) {
    *should_store_alias_results = true;
    return false;
  }
};

bool should_store_alias_results(const IRInstruction* instruction) {
  InstructionAnalyzerCombiner<ShouldStoreAliasResults> instruction_analyzer;
  bool should_store_alias_results = false;
  instruction_analyzer(instruction, &should_store_alias_results);
  return should_store_alias_results;
}

void store_alias_results(
    MethodContext& context,
    const MemoryLocationEnvironment& pre_memory_location_environment,
    const ForwardAliasEnvironment& post_alias_environment,
    const IRInstruction* instruction) {
  RegisterMemoryLocationsMap register_memory_locations_map =
      memory_location_map_from_environment(
          pre_memory_location_environment, instruction);

  std::optional<MemoryLocationsDomain> result_memory_locations = std::nullopt;
  if (instruction->has_dest()) {
    result_memory_locations =
        post_alias_environment.memory_locations(instruction->dest());
  } else if (instruction->has_move_result_any()) {
    result_memory_locations =
        post_alias_environment.memory_locations(k_result_register);
  }

  context.aliasing.store(
      instruction,
      InstructionAliasResults{
          std::move(register_memory_locations_map),
          post_alias_environment.make_widening_resolver(),
          result_memory_locations,
          post_alias_environment.last_position()});
}

void analyze_instruction(
    MethodContext& context,
    const InstructionAnalyzer<ForwardAliasEnvironment>& instruction_analyzer,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* alias_environment) {
  MemoryLocationEnvironment pre_memory_location_environment =
      alias_environment->memory_location_environment();

  LOG(5,
      "Analyzing instruction {} with environment: \n{}",
      show(instruction),
      *alias_environment);

  // Calls into ForwardAliasTransfer::analyze_xxx
  instruction_analyzer(instruction, alias_environment);

  if (should_store_alias_results(instruction)) {
    store_alias_results(
        context,
        pre_memory_location_environment,
        *alias_environment,
        instruction);
  }
}

} // namespace

void ForwardAliasFixpoint::analyze_node(
    const NodeId& block,
    ForwardAliasEnvironment* environment) const {
  LOG(4, "Analyzing block {}\n{}", block->id(), *environment);

  auto duration = timer_.duration_in_seconds();
  auto maximum_method_analysis_time =
      context_.options.maximum_method_analysis_time().value_or(
          std::numeric_limits<int>::max());
  if (duration > maximum_method_analysis_time) {
    throw TimeoutError(
        fmt::format(
            "Forward alias analysis of `{}` exceeded timeout of {}s.",
            context_.method()->show(),
            maximum_method_analysis_time),
        duration);
  }

  for (const auto& instruction : *block) {
    switch (instruction.type) {
      case MFLOW_OPCODE:
        analyze_instruction(
            context_, instruction_analyzer_, instruction.insn, environment);
        break;
      case MFLOW_POSITION:
        environment->set_last_position(instruction.pos.get());
        break;
      default:
        break;
    }
  }
}

ForwardAliasEnvironment ForwardAliasFixpoint::analyze_edge(
    const EdgeId& /*edge*/,
    const ForwardAliasEnvironment& environment) const {
  return environment;
}

} // namespace marianatrench
