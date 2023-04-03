/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/BackwardTaintTransfer.h>
#include <mariana-trench/CallGraph.h>
#include <mariana-trench/Features.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/PartialKind.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/TransferCall.h>
#include <mariana-trench/TransformOperations.h>
#include <mariana-trench/TriggeredPartialKind.h>

namespace marianatrench {

bool BackwardTaintTransfer::analyze_default(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* /* environment */) {
  log_instruction(context, instruction);

  // This is a no-op for backward taint.
  return false;
}

bool BackwardTaintTransfer::analyze_check_cast(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  auto taint = environment->read(aliasing.result_memory_location());

  // Add via-cast feature as configured by the program options.
  auto allowed_features = context->options.allow_via_cast_features();
  if (context->options.emit_all_via_cast_features() ||
      std::find(
          allowed_features.begin(),
          allowed_features.end(),
          instruction->get_type()->str()) != allowed_features.end()) {
    auto features = FeatureMayAlwaysSet::make_always(
        {context->features.get_via_cast_feature(instruction->get_type())});
    taint.map(
        [&features](Taint& sinks) { sinks.add_inferred_features(features); });
  }

  LOG_OR_DUMP(
      context, 4, "Tainting register {} with {}", instruction->src(0), taint);
  environment->write(
      aliasing.register_memory_locations(instruction->src(0)),
      std::move(taint),
      UpdateKind::Strong);

  return false;
}

bool BackwardTaintTransfer::analyze_iget(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* /* environment */) {
  log_instruction(context, instruction);

  // This is a no-op for backward taint.
  // We have field sources (see forward analysis) but no field sinks for now.
  return false;
}

bool BackwardTaintTransfer::analyze_sget(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* /* environment */) {
  log_instruction(context, instruction);

  // This is a no-op for backward taint.
  // We have field sources (see forward analysis) but no field sinks for now.
  return false;
}

namespace {

void apply_add_features_to_arguments(
    MethodContext* context,
    const InstructionAliasResults& aliasing,
    const BackwardTaintEnvironment* previous_environment,
    BackwardTaintEnvironment* new_environment,
    const IRInstruction* instruction,
    const CalleeModel& callee) {
  if (!callee.model.add_via_obscure_feature() &&
      !callee.model.has_add_features_to_arguments()) {
    return;
  }

  LOG_OR_DUMP(
      context, 4, "Processing add-via-obscure or add-features-to-arguments");

  for (std::size_t parameter_position = 0,
                   number_parameters = instruction->srcs_size();
       parameter_position < number_parameters;
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

    auto register_id = instruction->src(parameter_position);
    auto memory_locations = aliasing.register_memory_locations(register_id);
    for (auto* memory_location : memory_locations.elements()) {
      auto taint = previous_environment->read(memory_location);
      taint.map([&features, position](Taint& sinks) {
        sinks.add_inferred_features_and_local_position(features, position);
      });
      new_environment->write(
          memory_location, std::move(taint), UpdateKind::Strong);
    }
  }
}

void apply_propagations(
    MethodContext* context,
    const InstructionAliasResults& aliasing,
    const BackwardTaintEnvironment* previous_environment,
    BackwardTaintEnvironment* new_environment,
    const IRInstruction* instruction,
    const CalleeModel& callee,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    const TaintTree& result_taint) {
  LOG_OR_DUMP(
      context,
      4,
      "Processing propagations for call to `{}`",
      show(callee.method_reference));

  for (const auto& [input, propagations] :
       callee.model.propagations().elements()) {
    LOG_OR_DUMP(context, 4, "Processing propagations from {}", input);
    if (!input.root().is_argument()) {
      WARNING_OR_DUMP(
          context,
          2,
          "Ignoring propagation with non-argument input: {}",
          input);
      continue;
    }

    auto input_parameter_position = input.root().parameter_position();
    if (input_parameter_position >= instruction->srcs_size()) {
      WARNING(
          2,
          "Model for method `{}` contains a port on parameter {} but the method only has {} parameters. Skipping...",
          input_parameter_position,
          show(callee.method_reference),
          instruction->srcs_size());
      continue;
    }

    auto input_register_id = instruction->src(input_parameter_position);
    auto input_path_resolved = input.path().resolve(source_constant_arguments);
    auto* position =
        context->positions.get(callee.position, input.root(), instruction);

    for (const auto& propagation : propagations.frames_iterator()) {
      LOG_OR_DUMP(
          context,
          4,
          "Processing propagation from {} to {}",
          input,
          propagation);

      const PropagationKind* propagation_kind = propagation.propagation_kind();
      auto output_root = propagation_kind->root();

      TaintTree output_taint_tree = TaintTree::bottom();
      switch (output_root.kind()) {
        case Root::Kind::Return: {
          output_taint_tree = result_taint;
          break;
        }
        case Root::Kind::Argument: {
          auto output_parameter_position = output_root.parameter_position();
          auto output_register_id = instruction->src(output_parameter_position);
          output_taint_tree = previous_environment->read(
              aliasing.register_memory_locations(output_register_id));
          break;
        }
        default:
          mt_unreachable();
      }

      if (output_taint_tree.is_bottom()) {
        continue;
      }

      output_taint_tree = transforms::apply_propagation(
          context, propagation, std::move(output_taint_tree));

      FeatureMayAlwaysSet features = FeatureMayAlwaysSet::make_always(
          callee.model.add_features_to_arguments(output_root));
      features.add(propagation.features());
      features.add_always(callee.model.add_features_to_arguments(input.root()));

      output_taint_tree.map([&features, position](Taint& taints) {
        taints.add_inferred_features_and_local_position(features, position);
      });

      for (const auto& [output_path, _] :
           propagation.output_paths().elements()) {
        auto output_path_resolved =
            output_path.resolve(source_constant_arguments);

        auto input_taint_tree = output_taint_tree.read(
            output_path_resolved,
            BackwardTaintEnvironment::propagate_output_path);

        // Collapsing the tree here is required for correctness and performance.
        // Propagations can be collapsed, which results in taking the common
        // prefix of the input paths. Because of this, if we don't collapse
        // here, we might build invalid trees. See the end-to-end test
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
          LOG_OR_DUMP(context, 4, "Collapsing taint tree {}", input_taint_tree);
          input_taint_tree.collapse_inplace(
              /* transform */ [context](Taint& taint) {
                taint.add_inferred_features(FeatureMayAlwaysSet{
                    context->features.get_propagation_broadening_feature()});
              });
        }

        LOG_OR_DUMP(
            context,
            4,
            "Tainting register {} path {} with {}",
            input_register_id,
            input_path_resolved,
            input_taint_tree);
        new_environment->write(
            aliasing.register_memory_locations(input_register_id),
            input_path_resolved,
            std::move(input_taint_tree),
            callee.model.strong_write_on_propagation() ? UpdateKind::Strong
                                                       : UpdateKind::Weak);
      }
    }
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

void check_call_flows(
    MethodContext* context,
    const InstructionAliasResults& aliasing,
    BackwardTaintEnvironment* environment,
    const std::function<std::optional<Register>(ParameterPosition)>&
        get_parameter_register,
    const CalleeModel& callee,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    const FeatureMayAlwaysSet& extra_features,
    const FulfilledPartialKindState& fulfilled_partial_sinks) {
  LOG_OR_DUMP(
      context,
      4,
      "Processing sinks for call to `{}`",
      show(callee.method_reference));

  for (const auto& [port, sinks] : callee.model.sinks().elements()) {
    if (!port.root().is_argument()) {
      continue;
    }

    auto register_id = get_parameter_register(port.root().parameter_position());
    if (!register_id) {
      continue;
    }

    auto path_resolved = port.path().resolve(source_constant_arguments);

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
              /* unfulfilled_kind */ partial_sink, context->kinds);
        },
        [&fulfilled_partial_sinks](const Kind* new_kind) {
          return get_fulfilled_sink_features(fulfilled_partial_sinks, new_kind);
        });
    new_sinks.add_inferred_features(extra_features);

    LOG_OR_DUMP(
        context,
        4,
        "Tainting register {} path {} with {}",
        *register_id,
        path_resolved,
        new_sinks);
    environment->write(
        aliasing.register_memory_locations(*register_id),
        path_resolved,
        std::move(new_sinks),
        UpdateKind::Weak);
  }
}

void check_call_flows(
    MethodContext* context,
    const InstructionAliasResults& aliasing,
    BackwardTaintEnvironment* environment,
    const std::vector<Register>& instruction_sources,
    const CalleeModel& callee,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    const FeatureMayAlwaysSet& extra_features,
    const FulfilledPartialKindState& fulfilled_partial_sinks) {
  check_call_flows(
      context,
      aliasing,
      environment,
      [&instruction_sources](
          ParameterPosition parameter_position) -> std::optional<Register> {
        if (parameter_position >= instruction_sources.size()) {
          return std::nullopt;
        }

        return instruction_sources.at(parameter_position);
      },
      callee,
      source_constant_arguments,
      extra_features,
      fulfilled_partial_sinks);
}

void check_artificial_calls_flows(
    MethodContext* context,
    const InstructionAliasResults& aliasing,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment,
    const std::vector<std::optional<std::string>>& source_constant_arguments =
        {}) {
  const auto& artificial_callees =
      context->call_graph.artificial_callees(context->method(), instruction);

  for (const auto& artificial_callee : artificial_callees) {
    check_call_flows(
        context,
        aliasing,
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
        get_callee(context, artificial_callee, aliasing.position()),
        source_constant_arguments,
        FeatureMayAlwaysSet::make_always(artificial_callee.features),
        context->fulfilled_partial_sinks.get_artificial_call(
            &artificial_callee));
  }
}

void check_flows_to_array_allocation(
    MethodContext* context,
    const InstructionAliasResults& aliasing,
    BackwardTaintEnvironment* environment,
    const IRInstruction* instruction) {
  if (!context->artificial_methods.array_allocation_kind_used()) {
    return;
  }

  auto* array_allocation_method = context->methods.get(
      context->artificial_methods.array_allocation_method());
  auto* position =
      context->positions.get(context->method(), aliasing.position());
  auto array_allocation_sink = Taint{TaintConfig(
      /* kind */ context->artificial_methods.array_allocation_kind(),
      /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
      /* callee */ nullptr,
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
      /* canonical_names */ {},
      /* output_paths */ {},
      /* local_positions */ {},
      /* call_info */ CallInfo::Origin)};
  for (std::size_t parameter_position = 0,
                   number_parameters = instruction->srcs_size();
       parameter_position < number_parameters;
       parameter_position++) {
    auto register_id = instruction->src(parameter_position);
    environment->write(
        aliasing.register_memory_locations(register_id),
        array_allocation_sink,
        UpdateKind::Weak);
  }
}

} // namespace

bool BackwardTaintTransfer::analyze_invoke(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  auto source_constant_arguments = get_source_constant_arguments(
      aliasing.register_memory_locations_map(), instruction);

  check_artificial_calls_flows(
      context, aliasing, instruction, environment, source_constant_arguments);

  const BackwardTaintEnvironment previous_environment = *environment;

  auto callee = get_callee(
      context,
      instruction,
      aliasing.position(),
      get_source_register_types(context, instruction),
      source_constant_arguments);

  TaintTree result_taint = TaintTree::bottom();
  if (callee.resolved_base_method &&
      callee.resolved_base_method->returns_void()) {
    // No result.
  } else if (
      try_inline_invoke(
          context,
          aliasing.register_memory_locations_map(),
          instruction,
          callee) != nullptr) {
    // Since we are inlining the call, we should NOT propagate result taint.
    LOG_OR_DUMP(context, 4, "Inlining method call");
  } else {
    result_taint = previous_environment.read(aliasing.result_memory_location());
  }

  apply_add_features_to_arguments(
      context,
      aliasing,
      &previous_environment,
      environment,
      instruction,
      callee);
  check_call_flows(
      context,
      aliasing,
      environment,
      instruction->srcs_vec(),
      callee,
      source_constant_arguments,
      /* extra_features */ {},
      context->fulfilled_partial_sinks.get_call(instruction));
  apply_propagations(
      context,
      aliasing,
      &previous_environment,
      environment,
      instruction,
      callee,
      source_constant_arguments,
      result_taint);

  return false;
}

namespace {

void check_flows_to_field_sink(
    MethodContext* context,
    const InstructionAliasResults& aliasing,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  mt_assert(
      opcode::is_an_sput(instruction->opcode()) ||
      opcode::is_an_iput(instruction->opcode()));

  const auto field_target =
      context->call_graph.resolved_field_access(context->method(), instruction);
  if (!field_target) {
    WARNING_OR_DUMP(
        context,
        3,
        "Unable to resolve access of field {} for instruction opcode {}",
        show(instruction->get_field()),
        instruction->opcode());
    return;
  }

  auto field_model =
      field_target ? context->registry.get(field_target->field) : FieldModel();
  auto sinks = field_model.sinks();
  if (sinks.empty()) {
    return;
  }

  LOG_OR_DUMP(
      context, 4, "Tainting register {} with {}", instruction->src(0), sinks);
  environment->write(
      aliasing.register_memory_locations(instruction->src(0)),
      std::move(sinks),
      UpdateKind::Weak);
}

} // namespace

bool BackwardTaintTransfer::analyze_iput(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  check_artificial_calls_flows(context, aliasing, instruction, environment);

  auto* field_name = instruction->get_field()->get_name();
  auto target_memory_locations =
      aliasing.register_memory_locations(instruction->src(1));

  TaintTree target_taint = TaintTree::bottom();
  for (auto* memory_location : target_memory_locations.elements()) {
    auto* field_memory_location = memory_location->make_field(field_name);
    auto taint = environment->read(field_memory_location);
    add_field_features(context, taint, field_memory_location);
    target_taint.join_with(taint);
  }

  if (auto* target_memory_location = target_memory_locations.singleton();
      target_memory_location != nullptr) {
    auto* field_memory_location =
        (*target_memory_location)->make_field(field_name);
    LOG_OR_DUMP(
        context,
        4,
        "Clearing the taint for {}",
        show(field_memory_location),
        target_taint);
    environment->write(
        field_memory_location, TaintTree::bottom(), UpdateKind::Strong);
  }

  auto* position = context->positions.get(
      context->method(),
      aliasing.position(),
      Root(Root::Kind::Return),
      instruction);
  target_taint.map(
      [position](Taint& sinks) { sinks.add_local_position(position); });

  LOG_OR_DUMP(
      context,
      4,
      "Tainting register {} with {}",
      instruction->src(0),
      target_taint);
  environment->write(
      aliasing.register_memory_locations(instruction->src(0)),
      std::move(target_taint),
      UpdateKind::Weak);

  check_flows_to_field_sink(context, aliasing, instruction, environment);

  return false;
}

bool BackwardTaintTransfer::analyze_sput(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  check_flows_to_field_sink(context, aliasing, instruction, environment);
  return false;
}

namespace {

// Infer propagations and sinks for the backward `taint` on the given parameter.
void infer_input_taint(
    MethodContext* context,
    ParameterPosition parameter_position,
    const TaintTree& taint_tree) {
  auto input_root = Root(Root::Kind::Argument, parameter_position);

  for (const auto& [input_path, taint] : taint_tree.elements()) {
    auto partitioned_by_propagations = taint.partition_by_call_info<bool>(
        [](CallInfo call_info) { return call_info == CallInfo::Propagation; });

    auto sinks_iterator = partitioned_by_propagations.find(false);
    if (sinks_iterator != partitioned_by_propagations.end()) {
      auto& sinks = sinks_iterator->second;
      sinks.add_inferred_features(FeatureMayAlwaysSet::make_always(
          context->previous_model.attach_to_sinks(input_root)));
      auto port = AccessPath(input_root, input_path);
      LOG_OR_DUMP(context, 4, "Inferred sink for port {}: {}", port, sinks);
      context->new_model.add_inferred_sinks(
          std::move(port),
          std::move(sinks),
          /* widening_features */
          FeatureMayAlwaysSet{
              context->features.get_widen_broadening_feature()});
    }

    auto propagations_iterator = partitioned_by_propagations.find(true);
    if (propagations_iterator != partitioned_by_propagations.end()) {
      auto& propagations = propagations_iterator->second;

      if (parameter_position == 0 && !context->method()->is_static()) {
        // Do not infer propagations Arg(0) -> Arg(0).
        propagations.filter([](const Frame& frame) {
          auto* kind = frame.kind()->as<LocalArgumentKind>();
          return kind == nullptr || kind->parameter_position() != 0;
        });
      }

      if (propagations.is_bottom()) {
        continue;
      }

      propagations.map([context, &input_root](Frame& frame) {
        auto* propagation_kind = frame.propagation_kind();
        FeatureMayAlwaysSet features;
        features.add_always(
            context->previous_model.attach_to_propagations(input_root));
        features.add_always(context->previous_model.attach_to_propagations(
            propagation_kind->root()));
        frame.add_inferred_features(features);
      });

      auto input_port = AccessPath(input_root, input_path);
      LOG_OR_DUMP(
          context,
          4,
          "Inferred propagations from {} to {}",
          input_port,
          propagations);
      context->new_model.add_inferred_propagations(
          std::move(input_port),
          std::move(propagations),
          /* widening_features */
          FeatureMayAlwaysSet{
              context->features.get_widen_broadening_feature()});
    }
  }
}

} // namespace

bool BackwardTaintTransfer::analyze_load_param(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  auto* memory_location = aliasing.result_memory_location_or_null();
  if (memory_location == nullptr) {
    ERROR_OR_DUMP(context, 1, "Failed to deduce the parameter of a load");
    return false;
  }

  auto* parameter_memory_location =
      memory_location->as<ParameterMemoryLocation>();
  if (parameter_memory_location == nullptr) {
    ERROR_OR_DUMP(context, 1, "Failed to deduce the parameter of a load");
    return false;
  }

  infer_input_taint(
      context,
      parameter_memory_location->position(),
      environment->read(parameter_memory_location));

  return false;
}

bool BackwardTaintTransfer::analyze_move(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* /* environment */) {
  log_instruction(context, instruction);

  // This is a no-op for taint.
  return false;
}

bool BackwardTaintTransfer::analyze_move_result(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* /* environment */) {
  log_instruction(context, instruction);

  // This is a no-op for taint.
  return false;
}

bool BackwardTaintTransfer::analyze_aget(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* /* environment */) {
  log_instruction(context, instruction);

  // This is a no-op for taint.
  return false;
}

bool BackwardTaintTransfer::analyze_aput(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  auto taint = environment->read(
      aliasing.register_memory_locations(instruction->src(1)));

  auto features =
      FeatureMayAlwaysSet::make_always({context->features.get("via-array")});
  auto* position = context->positions.get(
      context->method(),
      aliasing.position(),
      Root(Root::Kind::Return),
      instruction);
  taint.map([&features, position](Taint& sources) {
    sources.add_inferred_features_and_local_position(features, position);
  });

  LOG_OR_DUMP(
      context, 4, "Tainting register {} with {}", instruction->src(0), taint);
  environment->write(
      aliasing.register_memory_locations(instruction->src(0)),
      std::move(taint),
      UpdateKind::Weak);

  return false;
}

bool BackwardTaintTransfer::analyze_new_array(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  check_flows_to_array_allocation(
      context, context->aliasing.get(instruction), environment, instruction);
  return analyze_default(context, instruction, environment);
}

bool BackwardTaintTransfer::analyze_filled_new_array(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  check_flows_to_array_allocation(
      context, context->aliasing.get(instruction), environment, instruction);
  return analyze_default(context, instruction, environment);
}

namespace {

static bool analyze_numerical_operator(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  TaintTree taint = environment->read(aliasing.result_memory_location());

  auto features = FeatureMayAlwaysSet::make_always(
      {context->features.get("via-numerical-operator")});
  auto* position = context->positions.get(
      context->method(),
      aliasing.position(),
      Root(Root::Kind::Return),
      instruction);
  taint.map([&features, position](Taint& sources) {
    sources.add_inferred_features_and_local_position(features, position);
  });

  for (auto register_id : instruction->srcs()) {
    LOG_OR_DUMP(context, 4, "Tainting register {} with {}", register_id, taint);
    environment->write(
        aliasing.register_memory_locations(register_id),
        taint,
        UpdateKind::Weak);
  }

  return false;
}

} // namespace

bool BackwardTaintTransfer::analyze_unop(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  return analyze_numerical_operator(context, instruction, environment);
}

bool BackwardTaintTransfer::analyze_binop(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  return analyze_numerical_operator(context, instruction, environment);
}

bool BackwardTaintTransfer::analyze_binop_lit(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  return analyze_numerical_operator(context, instruction, environment);
}

bool BackwardTaintTransfer::analyze_return(
    MethodContext* context,
    const IRInstruction* instruction,
    BackwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  // Add return sinks.
  auto taint = context->previous_model.sinks().read(Root(Root::Kind::Return));

  auto* position =
      context->positions.get(context->method(), aliasing.position());
  taint.map(
      [position](Taint& sinks) { sinks = sinks.attach_position(position); });

  // Add local return.
  taint.join_with(TaintTree(Taint::propagation_taint(
      /* kind */ context->kinds.local_return(),
      /* output_paths */
      PathTreeDomain{{Path{}, SingletonAbstractDomain()}},
      /* inferred_features */ {},
      /* user_features */ {})));

  for (auto register_id : instruction->srcs()) {
    LOG_OR_DUMP(context, 4, "Tainting register {} with {}", register_id, taint);
    // Using a strong update here could override and remove the LocalArgument
    // taint on Argument(0), which is necessary to infer propagations to `this`.
    environment->write(
        aliasing.register_memory_locations(register_id),
        taint,
        UpdateKind::Weak);
  }

  return false;
}

} // namespace marianatrench
