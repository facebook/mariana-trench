/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <limits>

#include <fmt/format.h>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/CallGraph.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Features.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/FulfilledPartialKindState.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/PartialKind.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/Transfer.h>
#include <mariana-trench/TriggeredPartialKind.h>

namespace marianatrench {

namespace {

constexpr Register k_result_register = std::numeric_limits<Register>::max();

inline void log_instruction(
    const MethodContext* context,
    const IRInstruction* instruction) {
  LOG_OR_DUMP(context, 4, "Instruction: \033[33m{}\033[0m", show(instruction));
}

} // namespace

bool Transfer::analyze_default(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);

  // Assign the result register to a new memory location.
  auto* memory_location = context->memory_factory.make_location(instruction);
  if (instruction->has_dest()) {
    LOG_OR_DUMP(
        context,
        4,
        "Setting register {} to {}",
        instruction->dest(),
        show(memory_location));
    environment->assign(instruction->dest(), memory_location);
  } else if (instruction->has_move_result_any()) {
    LOG_OR_DUMP(
        context, 4, "Setting result register to {}", show(memory_location));
    environment->assign(k_result_register, memory_location);
  } else {
    return false;
  }

  LOG_OR_DUMP(context, 4, "Tainting {} with {{}}", show(memory_location));
  environment->write(memory_location, TaintTree::bottom(), UpdateKind::Strong);

  return false;
}

bool Transfer::analyze_check_cast(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs().size() == 1);

  // Add via-cast feature
  auto taint = environment->read(instruction->srcs()[0]);
  auto features = FeatureMayAlwaysSet::make_always(
      {context->features.get_via_cast_feature(instruction->get_type())});
  taint.map(
      [&features](Taint& sources) { sources.add_inferred_features(features); });

  // Create a new memory location as we do not want to alias the pre-cast
  // location when attaching the via-cast feature.
  auto memory_location = context->memory_factory.make_location(instruction);
  environment->write(memory_location, taint, UpdateKind::Strong);

  LOG_OR_DUMP(
      context,
      4,
      "Setting result register to new memory location {}",
      show(memory_location));
  environment->assign(k_result_register, memory_location);

  return false;
}

bool Transfer::analyze_iget(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs().size() == 1);
  mt_assert(instruction->has_field());

  const auto field_target =
      context->call_graph.resolved_field_access(context->method(), instruction);
  if (!field_target) {
    WARNING_OR_DUMP(
        context,
        3,
        "Unable to resolve access of instance field {}",
        show(instruction->get_field()));
  }
  auto field_model =
      field_target ? context->registry.get(field_target->field) : FieldModel();

  // Create a memory location that represents the field.
  auto memory_locations = environment->memory_locations(
      /* register */ instruction->srcs()[0],
      /* field */ instruction->get_field()->get_name());
  LOG_OR_DUMP(context, 4, "Setting result register to {}", memory_locations);
  environment->assign(k_result_register, memory_locations);
  if (!field_model.empty()) {
    LOG_OR_DUMP(
        context,
        4,
        "Tainting register {} with {}",
        k_result_register,
        field_model.sources());
    environment->write(
        k_result_register, Path({}), field_model.sources(), UpdateKind::Strong);
  }

  return false;
}

bool Transfer::analyze_sget(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs().size() == 0);
  mt_assert(instruction->has_field());

  const auto field_target =
      context->call_graph.resolved_field_access(context->method(), instruction);
  if (!field_target) {
    WARNING_OR_DUMP(
        context,
        3,
        "Unable to resolve access of static field {}",
        show(instruction->get_field()));
  }
  auto field_model =
      field_target ? context->registry.get(field_target->field) : FieldModel();
  auto memory_location = context->memory_factory.make_location(instruction);
  LOG_OR_DUMP(context, 4, "Setting result register to {}", *memory_location);
  environment->assign(k_result_register, memory_location);
  if (!field_model.empty()) {
    LOG_OR_DUMP(
        context,
        4,
        "Tainting register {} with {}",
        k_result_register,
        field_model.sources());
    environment->write(
        k_result_register,
        TaintTree(field_model.sources()),
        UpdateKind::Strong);
  }

  return false;
}

namespace {

struct Callee {
  const DexMethodRef* method_reference;
  const Method* MT_NULLABLE resolved_base_method;
  const Position* position;
  Model model;
};

const std::vector<const DexType * MT_NULLABLE> get_source_register_types(
    const MethodContext* context,
    const IRInstruction* instruction) {
  std::vector<const DexType* MT_NULLABLE> register_types = {};
  for (const auto& source_register : instruction->srcs_vec()) {
    register_types.push_back(context->types.register_type(
        context->method(), instruction, source_register));
  }
  return register_types;
}

const std::vector<std::optional<std::string>> get_source_constant_arguments(
    AnalysisEnvironment* environment,
    const IRInstruction* instruction) {
  std::vector<std::optional<std::string>> constant_arguments = {};

  for (const auto& register_id : instruction->srcs_vec()) {
    auto memory_locations = environment->memory_locations(register_id);
    for (auto* memory_location : memory_locations.elements()) {
      std::optional<std::string> value;
      if (const auto* instruction_memory_location =
              memory_location->dyn_cast<InstructionMemoryLocation>();
          instruction_memory_location != nullptr) {
        value = instruction_memory_location->get_constant();
      }
      constant_arguments.push_back(std::move(value));
    }
  }

  return constant_arguments;
}

Callee get_callee(
    MethodContext* context,
    AnalysisEnvironment* environment,
    const IRInstruction* instruction) {
  mt_assert(opcode::is_an_invoke(instruction->opcode()));

  auto call_target = context->call_graph.callee(context->method(), instruction);
  if (!call_target.resolved()) {
    WARNING_OR_DUMP(
        context,
        3,
        "Unable to resolve call to `{}`",
        show(instruction->get_method()));
  } else {
    LOG_OR_DUMP(
        context,
        4,
        "Call resolved to `{}`",
        show(call_target.resolved_base_callee()));
  }

  auto* position =
      context->positions.get(context->method(), environment->last_position());

  auto model = context->model_at_callsite(
      call_target,
      position,
      get_source_register_types(context, instruction),
      get_source_constant_arguments(environment, instruction));
  LOG_OR_DUMP(context, 4, "Callee model: {}", model);

  // Avoid copies using `std::move`.
  // https://fb.workplace.com/groups/2292641227666517/permalink/2478196942444277/
  return Callee{
      instruction->get_method(),
      call_target.resolved_base_callee(),
      position,
      std::move(model)};
}

Callee get_callee(
    MethodContext* context,
    AnalysisEnvironment* environment,
    const ArtificialCallee& callee) {
  const auto* resolved_base_callee = callee.call_target.resolved_base_callee();
  mt_assert(resolved_base_callee != nullptr);

  LOG_OR_DUMP(
      context, 4, "Artificial call to `{}`", show(resolved_base_callee));

  auto* position =
      context->positions.get(context->method(), environment->last_position());

  auto model = context->model_at_callsite(
      callee.call_target,
      position,
      /* source_register_types */ {},
      /* source_constant_arguments */ {});
  LOG_OR_DUMP(context, 4, "Callee model: {}", model);

  return Callee{
      resolved_base_callee->dex_method(),
      resolved_base_callee,
      position,
      std::move(model)};
}

void apply_generations(
    MethodContext* context,
    AnalysisEnvironment* environment,
    const IRInstruction* instruction,
    const Callee& callee,
    TaintTree& result_taint) {
  const auto& instruction_sources = instruction->srcs_vec();

  LOG_OR_DUMP(
      context,
      4,
      "Processing generations for call to `{}`",
      show(callee.method_reference));

  for (const auto& [root, generations] : callee.model.generations()) {
    switch (root.kind()) {
      case Root::Kind::Return: {
        LOG_OR_DUMP(context, 4, "Tainting invoke result with {}", generations);
        result_taint.join_with(generations);
        break;
      }
      case Root::Kind::Argument: {
        auto parameter_position = root.parameter_position();
        auto register_id = instruction_sources.at(parameter_position);
        LOG_OR_DUMP(
            context,
            4,
            "Tainting register {} with {}",
            register_id,
            generations);
        environment->write(register_id, generations, UpdateKind::Weak);
        break;
      }
      default:
        mt_unreachable();
    }
  }
}

void apply_propagations(
    MethodContext* context,
    const AnalysisEnvironment* previous_environment,
    AnalysisEnvironment* new_environment,
    const IRInstruction* instruction,
    const Callee& callee,
    TaintTree& result_taint) {
  const auto& instruction_sources = instruction->srcs_vec();

  LOG_OR_DUMP(
      context,
      4,
      "Processing propagations for call to `{}`",
      show(callee.method_reference));

  for (const auto& [output, propagations] :
       callee.model.propagations().elements()) {
    auto output_features = FeatureMayAlwaysSet::make_always(
        callee.model.add_features_to_arguments(output.root()));

    for (const auto& propagation : propagations) {
      LOG_OR_DUMP(
          context, 4, "Processing propagation {} to {}", propagation, output);

      const auto& input = propagation.input().root();
      if (!input.is_argument()) {
        WARNING_OR_DUMP(
            context, 2, "Ignoring propagation with a return input: {}", input);
        continue;
      }

      auto input_parameter_position = input.parameter_position();
      if (input_parameter_position >= instruction_sources.size()) {
        WARNING(
            2,
            "Model for method `{}` contains a port on parameter {} but the method only has {} parameters. Skipping...",
            input_parameter_position,
            show(callee.method_reference),
            instruction_sources.size());

        continue;
      }
      auto input_register_id = instruction_sources.at(input_parameter_position);

      auto taint_tree = previous_environment->read(
          input_register_id, propagation.input().path());
      // Collapsing the tree here is required for correctness and performance.
      // Propagations can be collapsed, which results in taking the common
      // prefix of the input paths. Because of this, if we don't collapse here,
      // we might build invalid trees. See the end-to-end test
      // `propagation_collapse` for an example.
      // However, collapsing leads to FP with the builder pattern.
      // eg:
      // class A {
      //   private String s1;
      //
      //   public A setS1(String s) {
      //     this.s1 = s;
      //     return this;
      //   }
      // }
      // In this case, collapsing propagations results in entire `this` being
      // tainted. For chained calls, it can lead to FP.
      // `no-collapse-on-propagation` mode is used to prevent such cases.
      // See the end-to-end test `no_collapse_on_propagation` for example.
      if (!callee.model.no_collapse_on_propagation()) {
        LOG_OR_DUMP(context, 4, "Collapsing taint tree {}", taint_tree);
        taint_tree.collapse_inplace();
      }

      if (taint_tree.is_bottom()) {
        continue;
      }

      FeatureMayAlwaysSet features = output_features;
      features.add(propagation.features());
      features.add_always(callee.model.add_features_to_arguments(input));

      auto position =
          context->positions.get(callee.position, input, instruction);

      taint_tree.map([&features, position](Taint& taints) {
        taints.add_inferred_features_and_local_position(features, position);
      });

      switch (output.root().kind()) {
        case Root::Kind::Return: {
          LOG_OR_DUMP(
              context,
              4,
              "Tainting invoke result path {} with {}",
              output.path(),
              taint_tree);
          result_taint.write(
              output.path(), std::move(taint_tree), UpdateKind::Weak);
          break;
        }
        case Root::Kind::Argument: {
          auto output_parameter_position = output.root().parameter_position();
          auto output_register_id =
              instruction_sources.at(output_parameter_position);
          LOG_OR_DUMP(
              context,
              4,
              "Tainting register {} path {} with {}",
              output_register_id,
              output.path(),
              taint_tree);
          new_environment->write(
              output_register_id,
              output.path(),
              std::move(taint_tree),
              UpdateKind::Weak);
          break;
        }
        default:
          mt_unreachable();
      }
    }
  }

  if (callee.model.add_via_obscure_feature() ||
      callee.model.has_add_features_to_arguments()) {
    for (std::size_t parameter_position = 0;
         parameter_position < instruction_sources.size();
         parameter_position++) {
      auto parameter = Root(Root::Kind::Argument, parameter_position);
      auto features = FeatureMayAlwaysSet::make_always(
          callee.model.add_features_to_arguments(parameter));
      const auto* position = !features.empty()
          ? context->positions.get(callee.position, parameter, instruction)
          : nullptr;
      if (callee.model.add_via_obscure_feature()) {
        features.add_always(context->features.get("via-obscure"));
      }

      if (features.empty()) {
        continue;
      }

      auto register_id = instruction_sources[parameter_position];
      auto memory_locations =
          previous_environment->memory_locations(register_id);
      for (auto* memory_location : memory_locations.elements()) {
        auto taint = new_environment->read(memory_location);
        taint.map([&features, position](Taint& sources) {
          sources.add_inferred_features_and_local_position(features, position);
        });
        new_environment->write(
            memory_location, std::move(taint), UpdateKind::Strong);
      }
    }
  }
}

void create_issue(
    MethodContext* context,
    Taint source,
    Taint sink,
    const Rule* rule,
    const Position* position,
    const FeatureMayAlwaysSet& extra_features) {
  source.add_inferred_features(
      context->class_properties.issue_features(context->method()));
  sink.add_inferred_features(extra_features);
  auto issue =
      Issue(Taint{std::move(source)}, Taint{std::move(sink)}, rule, position);
  LOG_OR_DUMP(context, 4, "Found issue: {}", issue);
  context->model.add_issue(std::move(issue));
}

// Called when a source is detected to be flowing into a partial sink for a
// multi source rule. The set of fulfilled sinks should be accumulated for
// each argument at a callsite (an invoke operation).
void check_multi_source_multi_sink_rules(
    MethodContext* context,
    const Kind* source_kind,
    const Taint& source,
    const Kind* sink_kind,
    const Taint& sink,
    FulfilledPartialKindState& fulfilled_partial_sinks,
    const MultiSourceMultiSinkRule* rule,
    const Position* position,
    const FeatureMayAlwaysSet& extra_features) {
  const auto* partial_sink = sink_kind->as<PartialKind>();
  mt_assert(partial_sink != nullptr);

  // Features found by this branch of the multi-source-sink flow. Should be
  // reported as part of the final issue discovered.
  auto features = source.features_joined();
  features.add(sink.features_joined());

  auto issue_sink_frame = fulfilled_partial_sinks.fulfill_kind(
      partial_sink, rule, features, context, sink);

  if (issue_sink_frame) {
    create_issue(
        context, source, *issue_sink_frame, rule, position, extra_features);
  } else {
    LOG_OR_DUMP(
        context,
        4,
        "Found source kind: {} flowing into partial sink: {}, rule code: {}",
        *source_kind,
        *partial_sink,
        rule->code());
  }
}

FeatureMayAlwaysSet get_fulfilled_sink_features(
    const FulfilledPartialKindState& fulfilled_partial_sinks,
    const Kind* transformed_sink_kind) {
  const auto* new_kind = transformed_sink_kind->as<TriggeredPartialKind>();
  // Called only after transform_kind_with_features creates a triggered kind,
  // so this must be a TriggeredPartialKind.
  mt_assert(new_kind != nullptr);
  const auto* rule = new_kind->rule();
  const auto* counterpart = fulfilled_partial_sinks.get_fulfilled_counterpart(
      /* unfulfilled_kind */ new_kind->partial_kind(), rule);

  // A triggered kind was created, so its counterpart must exist.
  mt_assert(counterpart != nullptr);
  return fulfilled_partial_sinks.get_features(counterpart, rule);
}

void create_sinks(
    MethodContext* context,
    const Taint& sources,
    const Taint& sinks,
    const FeatureMayAlwaysSet& extra_features = {},
    const FulfilledPartialKindState& fulfilled_partial_sinks = {}) {
  if (sources.is_bottom() || sinks.is_bottom()) {
    return;
  }

  auto partitioned_by_artificial_sources = sources.partition_by_kind<bool>(
      [&](const Kind* kind) { return kind == Kinds::artificial_source(); });
  auto artificial_sources = partitioned_by_artificial_sources.find(true);
  if (artificial_sources == partitioned_by_artificial_sources.end()) {
    // Sinks are created when artificial sources are found flowing into them.
    // No artificial sources, therefore no sinks.
    return;
  }

  for (const auto& artificial_source :
       artificial_sources->second.frames_iterator()) {
    auto features = extra_features;
    features.add_always(
        context->model.attach_to_sinks(artificial_source.callee_port().root()));
    features.add(artificial_source.features());

    auto new_sinks = sinks;
    new_sinks.transform_kind_with_features(
        [context, &fulfilled_partial_sinks](
            const Kind* sink_kind) -> std::vector<const Kind*> {
          const auto* partial_sink = sink_kind->as<PartialKind>();
          if (!partial_sink) {
            // No transformation. Keep sink as it is.
            return {sink_kind};
          }
          return fulfilled_partial_sinks.make_triggered_counterparts(
              context, /* unfulfilled_kind */ partial_sink);
        },
        [&fulfilled_partial_sinks](const Kind* new_kind) {
          return get_fulfilled_sink_features(fulfilled_partial_sinks, new_kind);
        });
    new_sinks.add_inferred_features(features);

    // local_positions() are specific to the callee position. Normally,
    // combining all of a Taint's local positions would be odd, but this method
    // creates sinks and should always be called for the same call position
    // (where the sink is).
    new_sinks.set_local_positions(artificial_sources->second.local_positions());

    LOG_OR_DUMP(
        context,
        4,
        "Inferred sink for port {}: {}",
        artificial_source.callee_port(),
        new_sinks);
    context->model.add_inferred_sinks(
        artificial_source.callee_port(), std::move(new_sinks));
  }
}

// Checks if the given sources/sinks fulfill any rule. If so, create an issue.
//
// If fulfilled_partial_sinks is non-null, also checks for multi-source rules
// (partial rules). If a partial rule is fulfilled, this converts a partial
// sink to a triggered sink and accumulates this list of triggered sinks. How
// these sinks should be handled depends on what happens at other sinks/ports
// within the same callsite/invoke. The caller MUST accumulate triggered sinks
// at the callsite then call create_sinks. Regular sinks are also not created in
// this mode.
//
// If fulfilled_partial_sinks is null, regular sinks will be created if an
// artificial source is found to be flowing into a sink.
void check_flows(
    MethodContext* context,
    const Taint& sources,
    const Taint& sinks,
    const Position* position,
    const FeatureMayAlwaysSet& extra_features,
    FulfilledPartialKindState* MT_NULLABLE fulfilled_partial_sinks) {
  if (sources.is_bottom() || sinks.is_bottom()) {
    return;
  }

  auto sources_by_kind = sources.partition_by_kind();
  auto sinks_by_kind = sinks.partition_by_kind();
  for (const auto& [source_kind, source_taint] : sources_by_kind) {
    if (source_kind == Kinds::artificial_source()) {
      continue;
    }

    for (const auto& [sink_kind, sink_taint] : sinks_by_kind) {
      // Check if this satisfies any rule. If so, create the issue.
      const auto& rules = context->rules.rules(source_kind, sink_kind);
      for (const auto* rule : rules) {
        create_issue(
            context, source_taint, sink_taint, rule, position, extra_features);
      }

      // Check if this satisfies any partial (multi-source/sink) rule.
      if (fulfilled_partial_sinks) {
        const auto* MT_NULLABLE partial_sink = sink_kind->as<PartialKind>();
        if (partial_sink) {
          const auto& partial_rules =
              context->rules.partial_rules(source_kind, partial_sink);
          for (const auto* partial_rule : partial_rules) {
            check_multi_source_multi_sink_rules(
                context,
                source_kind,
                source_taint,
                sink_kind,
                sink_taint,
                *fulfilled_partial_sinks,
                partial_rule,
                position,
                extra_features);
          }
        }
      }
    }
  }

  if (!fulfilled_partial_sinks) {
    create_sinks(context, sources, sinks, extra_features);
  }
}

void check_flows(
    MethodContext* context,
    const AnalysisEnvironment* environment,
    const std::function<std::optional<Register>(ParameterPosition)>&
        get_parameter_register,
    const Callee& callee,
    const FeatureMayAlwaysSet& extra_features = {}) {
  LOG_OR_DUMP(
      context,
      4,
      "Processing sinks for call to `{}`",
      show(callee.method_reference));

  FulfilledPartialKindState fulfilled_partial_sinks;
  std::vector<std::tuple<AccessPath, Taint, const Taint&>> port_sources_sinks;

  for (const auto& [port, sinks] : callee.model.sinks().elements()) {
    if (!port.root().is_argument()) {
      continue;
    }

    auto register_id = get_parameter_register(port.root().parameter_position());
    if (!register_id) {
      continue;
    }

    Taint sources = environment->read(*register_id, port.path()).collapse();
    check_flows(
        context,
        sources,
        sinks,
        callee.position,
        extra_features,
        &fulfilled_partial_sinks);

    port_sources_sinks.push_back(
        std::make_tuple(port, std::move(sources), std::cref(sinks)));
  }

  // Create the sinks, checking at each point, if any partial sinks should
  // become triggered. This must not happen in the loop above because we need
  // the full set of triggered sinks at all positions/port of the callsite.
  //
  // Example: callsite(partial_sink_A, triggered_sink_B).
  // Scenario: triggered_sink_B discovered in check_flows above when a source
  // flows into the argument.
  //
  // This next loop needs that information to convert partial_sink_A into a
  // triggered sink to be propagated if it is reachable via artifical sources.
  //
  // Outside of multi-source rules, this also creates regular sinks for the
  // method if an artificial source is found flowing into a sink.
  for (const auto& [port, sources, sinks] : port_sources_sinks) {
    create_sinks(
        context, sources, sinks, extra_features, fulfilled_partial_sinks);
  }
}

void check_flows(
    MethodContext* context,
    const AnalysisEnvironment* environment,
    const std::vector<Register>& instruction_sources,
    const Callee& callee,
    const FeatureMayAlwaysSet& extra_features = {}) {
  check_flows(
      context,
      environment,
      [&instruction_sources](
          ParameterPosition parameter_position) -> std::optional<Register> {
        if (parameter_position >= instruction_sources.size()) {
          return std::nullopt;
        }

        return instruction_sources.at(parameter_position);
      },
      callee,
      extra_features);
}

void check_flows_to_array_allocation(
    MethodContext* context,
    AnalysisEnvironment* environment,
    const IRInstruction* instruction) {
  auto* array_allocation_method = context->methods.get(
      context->artificial_methods.array_allocation_method());
  auto* position =
      context->positions.get(context->method(), environment->last_position());
  auto array_allocation_sink = Taint{Frame(
      /* kind */ context->artificial_methods.array_allocation_kind(),
      /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
      /* callee */ array_allocation_method,
      /* field_callee */ nullptr,
      /* call_position */ position,
      /* distance */ 1,
      /* origins */ MethodSet{array_allocation_method},
      /* field_origins */ {},
      /* inferred features */ {},
      /* locally_inferred_features */ {},
      /* user features */ {},
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {})};
  auto instruction_sources = instruction->srcs_vec();
  for (std::size_t parameter_position = 0;
       parameter_position < instruction_sources.size();
       parameter_position++) {
    auto register_id = instruction_sources.at(parameter_position);
    Taint sources = environment->read(register_id).collapse();
    // Fulfilled partial sinks ignored. No partial sinks for array allocation.
    check_flows(
        context,
        sources,
        array_allocation_sink,
        position,
        /* extra_features */ {},
        /* fulfilled_partial_sinks */ nullptr);
  }
}

void check_flows(
    MethodContext* context,
    const AnalysisEnvironment* environment,
    const IRInstruction* instruction,
    const Callee& callee) {
  check_flows(context, environment, instruction->srcs_vec(), callee);
}

void analyze_artificial_calls(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  const auto& artificial_callees =
      context->call_graph.artificial_callees(context->method(), instruction);

  for (const auto& artificial_callee : artificial_callees) {
    check_flows(
        context,
        environment,
        [&artificial_callee](
            ParameterPosition parameter_position) -> std::optional<Register> {
          auto found =
              artificial_callee.parameter_registers.find(parameter_position);
          if (found == artificial_callee.parameter_registers.end()) {
            return std::nullopt;
          }

          return found->second;
        },
        get_callee(context, environment, artificial_callee),
        FeatureMayAlwaysSet::make_always(artificial_callee.features));
  }
}

MemoryLocation* MT_NULLABLE try_alias_this_location(
    MethodContext* context,
    AnalysisEnvironment* environment,
    const Callee& callee,
    const IRInstruction* instruction) {
  if (!callee.model.alias_memory_location_on_invoke()) {
    return nullptr;
  }

  if (callee.resolved_base_method && callee.resolved_base_method->is_static()) {
    return nullptr;
  }

  auto register_id = instruction->srcs_vec().at(0);
  auto memory_locations = environment->memory_locations(register_id);
  if (!memory_locations.is_value() || memory_locations.size() != 1) {
    return nullptr;
  }

  auto* memory_location = *memory_locations.elements().begin();
  LOG_OR_DUMP(
      context,
      4,
      "Method invoke aliasing existing memory location {}",
      show(memory_location));

  return memory_location;
}

// If the method invoke can be safely inlined, return the result memory
// location, otherwise return nullptr.
MemoryLocation* MT_NULLABLE try_inline_invoke(
    MethodContext* context,
    const AnalysisEnvironment* environment,
    const IRInstruction* instruction,
    const Callee& callee) {
  auto access_path = callee.model.inline_as().get_constant();
  if (!access_path) {
    return nullptr;
  }

  auto register_id = instruction->src(access_path->root().parameter_position());
  auto memory_locations = environment->memory_locations(register_id);
  if (!memory_locations.is_value() || memory_locations.size() != 1) {
    return nullptr;
  }

  auto memory_location = *memory_locations.elements().begin();
  for (const auto* field : access_path->path()) {
    memory_location = memory_location->make_field(field);
  }

  // Only inline if the model does not generate or propagate extra taint.
  if (!callee.model.generations().is_bottom() ||
      !callee.model.propagations().leq(PropagationAccessPathTree({
          {AccessPath(Root(Root::Kind::Return)),
           PropagationSet{Propagation(
               /* input */ *access_path,
               /* inferred_features */ FeatureMayAlwaysSet(),
               /* user_features */ FeatureSet::bottom())}},
      })) ||
      callee.model.add_via_obscure_feature() ||
      callee.model.has_add_features_to_arguments()) {
    return nullptr;
  }

  LOG_OR_DUMP(context, 4, "Inlining method call");
  return memory_location;
}

} // namespace

bool Transfer::analyze_invoke(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);

  auto callee = get_callee(context, environment, instruction);

  const AnalysisEnvironment previous_environment = *environment;
  TaintTree result_taint;
  check_flows(context, &previous_environment, instruction, callee);
  apply_propagations(
      context,
      &previous_environment,
      environment,
      instruction,
      callee,
      result_taint);
  apply_generations(context, environment, instruction, callee, result_taint);

  if (callee.resolved_base_method &&
      callee.resolved_base_method->returns_void()) {
    LOG_OR_DUMP(context, 4, "Resetting the result register");
    environment->assign(k_result_register, MemoryLocationsDomain::bottom());
  } else if (
      auto* memory_location =
          try_inline_invoke(context, environment, instruction, callee)) {
    LOG_OR_DUMP(
        context, 4, "Setting result register to {}", show(memory_location));
    environment->assign(k_result_register, memory_location);
  } else {
    // Check if the method can alias existing memory location
    memory_location =
        try_alias_this_location(context, environment, callee, instruction);

    // Assume the method call returns a new memory location,
    // that does not alias with anything.
    if (memory_location == nullptr) {
      memory_location = context->memory_factory.make_location(instruction);
    }

    LOG_OR_DUMP(
        context, 4, "Setting result register to {}", show(memory_location));
    environment->assign(k_result_register, memory_location);

    LOG_OR_DUMP(
        context, 4, "Tainting {} with {}", show(memory_location), result_taint);
    environment->write(memory_location, result_taint, UpdateKind::Weak);
  }

  analyze_artificial_calls(context, instruction, environment);

  return false;
}

bool is_inner_class_this(const FieldMemoryLocation* location) {
  return location->parent()->is<ThisParameterMemoryLocation>() &&
      location->field()->str() == "this$0";
}

void add_field_features(
    MethodContext* context,
    AbstractTreeDomain<Taint>& taint,
    const FieldMemoryLocation* field_memory_location) {
  if (!is_inner_class_this(field_memory_location)) {
    return;
  }
  auto features = FeatureMayAlwaysSet::make_always(
      {context->features.get("via-inner-class-this")});
  taint.map(
      [&features](Taint& sources) { sources.add_inferred_features(features); });
}

bool Transfer::analyze_iput(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs().size() == 2);
  mt_assert(instruction->has_field());

  auto taint = environment->read(/* register */ instruction->srcs()[0]);

  auto* position = context->positions.get(
      context->method(),
      environment->last_position(),
      Root(Root::Kind::Return),
      instruction);
  taint.map(
      [position](Taint& sources) { sources.add_local_position(position); });

  // Check if the taint above flows into a field sink
  const auto field_target =
      context->call_graph.resolved_field_access(context->method(), instruction);
  if (!field_target) {
    WARNING_OR_DUMP(
        context,
        3,
        "Unable to resolve access of field for iput {}",
        show(instruction->get_field()));
  } else {
    auto field_model = field_target ? context->registry.get(field_target->field)
                                    : FieldModel();
    auto sinks = field_model.sinks();
    if (!sinks.empty() && !taint.is_bottom()) {
      for (const auto& [port, sources] : taint.elements()) {
        check_flows(
            context,
            sources,
            sinks,
            position,
            /* extra_features */ FeatureMayAlwaysSet(),
            /* fulfilled_partial_sinks */ nullptr);
      }
    }
  }

  // Store the taint in the memory location(s) representing the field
  auto* field_name = instruction->get_field()->get_name();
  auto target_memory_locations =
      environment->memory_locations(/* register */ instruction->srcs()[1]);
  bool is_singleton = target_memory_locations.elements().size() == 1;

  for (auto* memory_location : target_memory_locations.elements()) {
    auto field_memory_location = memory_location->make_field(field_name);
    auto taint_copy = taint;
    add_field_features(context, taint_copy, field_memory_location);

    LOG_OR_DUMP(
        context,
        4,
        "Tainting {} with {}",
        show(field_memory_location),
        taint_copy);
    environment->write(
        field_memory_location,
        taint_copy,
        is_singleton ? UpdateKind::Strong : UpdateKind::Weak);
  }

  analyze_artificial_calls(context, instruction, environment);

  return false;
}

bool Transfer::analyze_sput(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs().size() == 1);
  mt_assert(instruction->has_field());

  auto taint = environment->read(/* register */ instruction->srcs()[0]);
  if (taint.is_bottom()) {
    return false;
  }
  auto* position = context->positions.get(
      context->method(),
      environment->last_position(),
      Root(Root::Kind::Return),
      instruction);
  taint.map(
      [position](Taint& sources) { sources.add_local_position(position); });

  const auto field_target =
      context->call_graph.resolved_field_access(context->method(), instruction);
  if (!field_target) {
    WARNING_OR_DUMP(
        context,
        3,
        "Unable to resolve access of field for sput {}",
        show(instruction->get_field()));
    return false;
  }
  auto field_model =
      field_target ? context->registry.get(field_target->field) : FieldModel();
  auto sinks = field_model.sinks();
  if (sinks.empty()) {
    return false;
  }
  for (const auto& [port, sources] : taint.elements()) {
    check_flows(
        context,
        sources,
        sinks,
        position,
        /* extra_features */ FeatureMayAlwaysSet(),
        /* fulfilled_partial_sinks */ nullptr);
  }
  return false;
}

bool Transfer::analyze_load_param(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);

  auto abstract_parameter = environment->last_parameter_loaded();
  if (!abstract_parameter.is_value()) {
    ERROR_OR_DUMP(context, 1, "Failed to deduce the parameter of a load");
    return false;
  }
  auto parameter_position = *abstract_parameter.get_constant();
  environment->increment_last_parameter_loaded();

  // Create a memory location that represents the argument.
  auto memory_location =
      context->memory_factory.make_parameter(parameter_position);
  LOG_OR_DUMP(
      context,
      4,
      "Setting register {} to {}",
      instruction->dest(),
      show(memory_location));
  environment->assign(instruction->dest(), memory_location);

  // Add parameter sources specified in model generators.
  auto root = Root(Root::Kind::Argument, parameter_position);
  auto taint = context->model.parameter_sources().read(root);

  // Add the position of the instruction to the parameter sources.
  auto* position = context->positions.get(context->method());
  taint.map([position](Taint& sources) {
    sources = sources.attach_position(position);
  });

  // Introduce an artificial parameter source in order to infer sinks and
  // propagations.
  taint.write(
      Path{},
      Taint{Frame::artificial_source(AccessPath(root))},
      UpdateKind::Weak);

  LOG_OR_DUMP(context, 4, "Tainting {} with {}", show(memory_location), taint);
  environment->write(memory_location, std::move(taint), UpdateKind::Strong);

  return false;
}

bool Transfer::analyze_move(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs().size() == 1);

  auto memory_locations =
      environment->memory_locations(/* register */ instruction->srcs()[0]);
  LOG_OR_DUMP(
      context,
      4,
      "Setting register {} to {}",
      instruction->dest(),
      memory_locations);
  environment->assign(instruction->dest(), memory_locations);

  return false;
}

bool Transfer::analyze_move_result(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);

  auto memory_locations = environment->memory_locations(k_result_register);
  LOG_OR_DUMP(
      context,
      4,
      "Setting register {} to {}",
      instruction->dest(),
      memory_locations);
  environment->assign(instruction->dest(), memory_locations);

  LOG_OR_DUMP(context, 4, "Resetting the result register");
  environment->assign(k_result_register, MemoryLocationsDomain::bottom());

  return false;
}

bool Transfer::analyze_aget(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs().size() == 2);

  // We use a single memory location for the array and its elements.
  auto memory_locations =
      environment->memory_locations(/* register */ instruction->srcs()[0]);
  LOG_OR_DUMP(context, 4, "Setting result register to {}", memory_locations);
  environment->assign(k_result_register, memory_locations);

  return false;
}

bool Transfer::analyze_aput(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs().size() == 3);

  auto taint = environment->read(
      /* register */ instruction->srcs()[0]);

  auto features =
      FeatureMayAlwaysSet::make_always({context->features.get("via-array")});
  auto* position = context->positions.get(
      context->method(),
      environment->last_position(),
      Root(Root::Kind::Return),
      instruction);
  taint.map([&features, position](Taint& sources) {
    sources.add_inferred_features_and_local_position(features, position);
  });

  // We use a single memory location for the array and its elements.
  auto target_memory_locations =
      environment->memory_locations(/* register */ instruction->srcs()[1]);
  for (auto* memory_location : target_memory_locations.elements()) {
    LOG_OR_DUMP(
        context, 4, "Tainting {} with {}", show(memory_location), taint);
    environment->write(memory_location, taint, UpdateKind::Weak);
  }

  return false;
}

bool Transfer::analyze_new_array(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  check_flows_to_array_allocation(context, environment, instruction);
  return analyze_default(context, instruction, environment);
}

bool Transfer::analyze_filled_new_array(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  check_flows_to_array_allocation(context, environment, instruction);
  return analyze_default(context, instruction, environment);
}

namespace {

static bool analyze_numerical_operator(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);

  TaintTree taint;
  for (auto register_id : instruction->srcs()) {
    taint.join_with(environment->read(register_id));
  }

  auto features = FeatureMayAlwaysSet::make_always(
      {context->features.get("via-numerical-operator")});
  auto* position = context->positions.get(
      context->method(),
      environment->last_position(),
      Root(Root::Kind::Return),
      instruction);
  taint.map([&features, position](Taint& sources) {
    sources.add_inferred_features_and_local_position(features, position);
  });

  // Assume the instruction creates a new memory location.
  auto memory_location = context->memory_factory.make_location(instruction);
  if (instruction->has_dest()) {
    LOG_OR_DUMP(
        context,
        4,
        "Setting register {} to {}",
        instruction->dest(),
        show(memory_location));
    environment->assign(instruction->dest(), memory_location);
  } else if (instruction->has_move_result_any()) {
    LOG_OR_DUMP(
        context, 4, "Setting result register to {}", show(memory_location));
    environment->assign(k_result_register, memory_location);
  } else {
    return false;
  }

  LOG_OR_DUMP(context, 4, "Tainting {} with {}", show(memory_location), taint);
  environment->write(memory_location, taint, UpdateKind::Strong);

  return false;
}

} // namespace

bool Transfer::analyze_unop(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  return analyze_numerical_operator(context, instruction, environment);
}

bool Transfer::analyze_binop(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  return analyze_numerical_operator(context, instruction, environment);
}

bool Transfer::analyze_binop_lit(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  return analyze_numerical_operator(context, instruction, environment);
}

namespace {

// Infer propagations and generations for the output `taint` on port `root`.
void infer_output_taint(
    MethodContext* context,
    Root root,
    const TaintTree& taint) {
  for (const auto& [path, sources] : taint.elements()) {
    auto partitioned_by_artificial_sources = sources.partition_by_kind<bool>(
        [&](const Kind* kind) { return kind == Kinds::artificial_source(); });

    auto real_sources = partitioned_by_artificial_sources.find(false);
    if (real_sources != partitioned_by_artificial_sources.end()) {
      auto generation = real_sources->second;
      generation.add_inferred_features(FeatureMayAlwaysSet::make_always(
          context->model.attach_to_sources(root)));
      auto port = AccessPath(root, path);
      LOG_OR_DUMP(
          context, 4, "Inferred generation for port {}: {}", port, generation);
      context->model.add_inferred_generations(
          std::move(port), std::move(generation));
    }

    auto artificial_sources = partitioned_by_artificial_sources.find(true);
    if (artificial_sources != partitioned_by_artificial_sources.end()) {
      for (const auto& artificial_source :
           artificial_sources->second.frames_iterator()) {
        if (artificial_source.callee_port().root() != root) {
          const auto& input = artificial_source.callee_port();
          auto output = AccessPath(root, path);
          auto features = artificial_source.features();
          features.add_always(
              context->model.attach_to_propagations(input.root()));
          features.add_always(context->model.attach_to_propagations(root));
          auto propagation = Propagation(
              input,
              /* inferred_features */ features,
              /* user_features */ FeatureSet::bottom());
          LOG_OR_DUMP(
              context, 4, "Inferred propagation {} to {}", propagation, output);
          context->model.add_inferred_propagation(
              std::move(propagation), std::move(output));
        }
      }
    }
  }
}

bool has_side_effect(const MethodItemEntry& instruction) {
  switch (instruction.type) {
    case MFLOW_OPCODE:
      switch (instruction.insn->opcode()) {
        case IOPCODE_LOAD_PARAM:
        case IOPCODE_LOAD_PARAM_OBJECT:
        case IOPCODE_LOAD_PARAM_WIDE:
        case OPCODE_NOP:
        case OPCODE_MOVE:
        case OPCODE_MOVE_WIDE:
        case OPCODE_MOVE_OBJECT:
        case OPCODE_MOVE_RESULT:
        case OPCODE_MOVE_RESULT_WIDE:
        case OPCODE_MOVE_RESULT_OBJECT:
        case IOPCODE_MOVE_RESULT_PSEUDO:
        case IOPCODE_MOVE_RESULT_PSEUDO_OBJECT:
        case IOPCODE_MOVE_RESULT_PSEUDO_WIDE:
        case OPCODE_RETURN_VOID:
        case OPCODE_RETURN:
        case OPCODE_RETURN_WIDE:
        case OPCODE_RETURN_OBJECT:
        case OPCODE_CONST:
        case OPCODE_CONST_WIDE:
        case OPCODE_IGET:
        case OPCODE_IGET_WIDE:
        case OPCODE_IGET_OBJECT:
        case OPCODE_IGET_BOOLEAN:
        case OPCODE_IGET_BYTE:
        case OPCODE_IGET_CHAR:
        case OPCODE_IGET_SHORT:
          return false;
        default:
          return true;
      }
      break;
    case MFLOW_DEBUG:
    case MFLOW_POSITION:
    case MFLOW_FALLTHROUGH:
      return false;
    default:
      return true;
  }
}

// Infer whether the method could be inlined.
AccessPathConstantDomain infer_inline_as(
    MethodContext* context,
    const MemoryLocationsDomain& memory_locations) {
  // Check if we are returning an argument access path.
  if (!memory_locations.is_value() || memory_locations.size() != 1 ||
      context->model.has_global_propagation_sanitizer()) {
    return AccessPathConstantDomain::top();
  }

  auto* memory_location = *memory_locations.elements().begin();
  auto access_path = memory_location->access_path();
  if (!access_path) {
    return AccessPathConstantDomain::top();
  }

  LOG_OR_DUMP(
      context, 4, "Instruction returns the access path: {}", *access_path);

  // Check if the method has any side effect.
  const auto* code = context->method()->get_code();
  mt_assert(code != nullptr);
  const auto& cfg = code->cfg();
  if (cfg.blocks().size() != 1) {
    // There could be multiple return statements.
    LOG_OR_DUMP(
        context, 4, "Method has multiple basic blocks, it cannot be inlined.");
    return AccessPathConstantDomain::top();
  }

  auto* entry_block = cfg.entry_block();
  auto found =
      std::find_if(entry_block->begin(), entry_block->end(), has_side_effect);
  if (found != entry_block->end()) {
    LOG_OR_DUMP(
        context,
        4,
        "Method has an instruction with possible side effects: {}, it cannot be inlined.",
        show(*found));
    return AccessPathConstantDomain::top();
  }

  LOG_OR_DUMP(context, 4, "Method can be inlined as {}", *access_path);
  return AccessPathConstantDomain(*access_path);
}

} // namespace

bool Transfer::analyze_return(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);

  auto return_sinks = context->model.sinks().read(Root(Root::Kind::Return));

  // Add the position of the instruction to the return sinks.
  auto* position =
      context->positions.get(context->method(), environment->last_position());
  return_sinks.map(
      [position](Taint& sinks) { sinks = sinks.attach_position(position); });

  for (auto register_id : instruction->srcs()) {
    auto memory_locations = environment->memory_locations(register_id);
    context->model.set_inline_as(infer_inline_as(context, memory_locations));
    infer_output_taint(
        context, Root(Root::Kind::Return), environment->read(memory_locations));

    for (const auto& [path, sinks] : return_sinks.elements()) {
      Taint sources = environment->read(register_id, path).collapse();
      // Fulfilled partial sinks are not expected to be produced here. Return
      // sinks are never partial.
      check_flows(
          context,
          sources,
          sinks,
          position,
          /* extra_features */ {},
          /* fulfilled_partial_sinks */ nullptr);
    }
  }

  if (!context->method()->is_static()) {
    infer_output_taint(
        context,
        Root(Root::Kind::Argument, 0),
        environment->read(context->memory_factory.make_parameter(0)));
  }

  return false;
}

} // namespace marianatrench
