// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <limits>

#include <fmt/format.h>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/CallGraph.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Features.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/Transfer.h>

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

  // This is similar to a move from our point of view.
  auto memory_locations =
      environment->memory_locations(/* register */ instruction->srcs()[0]);
  LOG_OR_DUMP(context, 4, "Setting result register to {}", memory_locations);
  environment->assign(k_result_register, memory_locations);

  return false;
}

bool Transfer::analyze_iget(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs().size() == 1);
  mt_assert(instruction->has_field());

  // Create a memory location that represents the field.
  auto memory_locations = environment->memory_locations(
      /* register */ instruction->srcs()[0],
      /* field */ instruction->get_field()->get_name());
  LOG_OR_DUMP(context, 4, "Setting result register to {}", memory_locations);
  environment->assign(k_result_register, memory_locations);

  return false;
}

namespace {

struct Callee {
  const DexMethodRef* method_reference;
  const Method* MT_NULLABLE resolved_base_method;
  const Position* position;
  Model model;
};

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

  auto model = context->model_at_callsite(call_target, position);
  LOG_OR_DUMP(context, 4, "Callee model: {}", model);

  // Avoid copies using `std::move`.
  // https://fb.workplace.com/groups/2292641227666517/permalink/2478196942444277/
  return Callee{instruction->get_method(),
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

  auto model = context->model_at_callsite(callee.call_target, position);
  LOG_OR_DUMP(context, 4, "Callee model: {}", model);

  return Callee{resolved_base_callee->dex_method(),
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
    if (callee.model.add_via_obscure_feature()) {
      output_features.add_always(context->features.get("via-obscure"));
    }

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
      auto input_register_id = instruction_sources.at(input_parameter_position);

      // Collapsing the tree here is required for correctness and performance.
      // Propagations can be collapsed, which results in taking the common
      // prefix of the input paths. Because of this, if we don't collapse here,
      // we might build invalid trees. See the end-to-end test
      // `propagation_collapse` for an example.
      auto taint = previous_environment
                       ->read(input_register_id, propagation.input().path())
                       .collapse();

      if (taint.is_bottom()) {
        continue;
      }

      FeatureMayAlwaysSet features = output_features;
      features.add(propagation.features());
      features.add_always(callee.model.add_features_to_arguments(input));

      taint.add_features_and_local_position(features, callee.position);

      switch (output.root().kind()) {
        case Root::Kind::Return: {
          LOG_OR_DUMP(
              context,
              4,
              "Tainting invoke result path {} with {}",
              output.path(),
              taint);
          result_taint.write(output.path(), std::move(taint), UpdateKind::Weak);
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
              taint);
          new_environment->write(
              output_register_id,
              output.path(),
              std::move(taint),
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
      auto features = FeatureMayAlwaysSet::make_always(
          callee.model.add_features_to_arguments(
              Root(Root::Kind::Argument, parameter_position)));
      const auto* position = !features.empty() ? callee.position : nullptr;
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
          sources.add_features_and_local_position(features, position);
        });
        new_environment->write(
            memory_location, std::move(taint), UpdateKind::Strong);
      }
    }
  }
}

void check_flows(
    MethodContext* context,
    const Taint& sources,
    const Taint& sinks,
    const FeatureMayAlwaysSet& extra_features = {}) {
  if (sources.is_bottom() || sinks.is_bottom()) {
    return;
  }

  for (const auto& sink : sinks) {
    for (const auto& source : sources) {
      if (!source.is_artificial_sources()) {
        const auto& rules = context->rules.rules(source.kind(), sink.kind());

        for (const auto* rule : rules) {
          auto new_sink = sink;
          new_sink.add_features(extra_features);

          auto issue = Issue(source, std::move(new_sink), rule);
          LOG_OR_DUMP(context, 4, "Found issue: {}", issue);
          context->model.add_issue(std::move(issue));
        }
      }
    }
  }

  for (const auto& source : sources) {
    if (source.is_artificial_sources()) {
      for (const auto& artificial_source : source) {
        auto features = extra_features;
        features.add_always(context->model.attach_to_sinks(
            artificial_source.callee_port().root()));
        features.add(artificial_source.features());

        auto new_sinks = sinks;
        new_sinks.add_features(features);
        new_sinks.set_local_positions(source.local_positions());

        LOG_OR_DUMP(
            context,
            4,
            "Inferred sink for port {}: {}",
            artificial_source.callee_port(),
            new_sinks);
        context->model.add_sinks(
            artificial_source.callee_port(), std::move(new_sinks));
      }
    }
  }
}

void check_flows(
    MethodContext* context,
    const AnalysisEnvironment* environment,
    const std::vector<Register>& instruction_sources,
    const Callee& callee,
    const FeatureMayAlwaysSet& extra_features = {}) {
  LOG_OR_DUMP(
      context,
      4,
      "Processing sinks for call to `{}`",
      show(callee.method_reference));

  for (const auto& [port, sinks] : callee.model.sinks().elements()) {
    if (!port.root().is_argument()) {
      continue;
    }

    auto parameter_position = port.root().parameter_position();
    if (parameter_position >= instruction_sources.size()) {
      continue;
    }

    auto register_id = instruction_sources.at(parameter_position);
    Taint sources = environment->read(register_id, port.path()).collapse();
    check_flows(context, sources, sinks, extra_features);
  }
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
      /* call_position */ position,
      /* distance */ 1,
      /* origins */ MethodSet{array_allocation_method},
      /* features */ {},
      /* local_positions */ {})};
  auto instruction_sources = instruction->srcs_vec();
  for (std::size_t parameter_position = 0;
       parameter_position < instruction_sources.size();
       parameter_position++) {
    auto register_id = instruction_sources.at(parameter_position);
    Taint sources = environment->read(register_id).collapse();
    check_flows(context, sources, array_allocation_sink);
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
        artificial_callee.register_parameters,
        get_callee(context, environment, artificial_callee),
        FeatureMayAlwaysSet::make_always(artificial_callee.features));
  }
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
  } else {
    // Assume the method call returns a new memory location,
    // that does not alias with anything.
    auto memory_location = context->memory_factory.make_location(instruction);
    LOG_OR_DUMP(
        context, 4, "Setting result register to {}", show(memory_location));
    environment->assign(k_result_register, memory_location);

    LOG_OR_DUMP(
        context, 4, "Tainting {} with {}", show(memory_location), result_taint);
    environment->write(memory_location, result_taint, UpdateKind::Strong);
  }

  analyze_artificial_calls(context, instruction, environment);

  return false;
}

bool Transfer::analyze_iput(
    MethodContext* context,
    const IRInstruction* instruction,
    AnalysisEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs().size() == 2);
  mt_assert(instruction->has_field());

  auto taint = environment->read(/* register */ instruction->srcs()[0]);

  auto* position =
      context->positions.get(context->method(), environment->last_position());
  taint.map(
      [position](Taint& sources) { sources.add_local_position(position); });

  auto* field = instruction->get_field()->get_name();
  auto target_memory_locations =
      environment->memory_locations(/* register */ instruction->srcs()[1]);
  bool is_singleton = target_memory_locations.elements().size() == 1;

  for (auto* memory_location : target_memory_locations.elements()) {
    auto field_memory_location = memory_location->make_field(field);
    LOG_OR_DUMP(
        context, 4, "Tainting {} with {}", show(field_memory_location), taint);
    environment->write(
        field_memory_location,
        taint,
        is_singleton ? UpdateKind::Strong : UpdateKind::Weak);
  }

  analyze_artificial_calls(context, instruction, environment);

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

  // Add special features that cannot be done in model generators.
  auto features =
      context->class_properties.parameter_source_features(context->method());

  // Add the position of the instruction to the parameter sources.
  auto* position =
      context->positions.get(context->method(), environment->last_position());
  taint.map([&features, position](Taint& sources) {
    sources = sources.attach_position(position, features);
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
  auto* position =
      context->positions.get(context->method(), environment->last_position());
  taint.map([&features, position](Taint& sources) {
    sources.add_features_and_local_position(features, position);
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
  auto* position =
      context->positions.get(context->method(), environment->last_position());
  taint.map([&features, position](Taint& sources) {
    sources.add_features_and_local_position(features, position);
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
    for (const auto& source : sources) {
      if (!source.is_artificial_sources()) {
        auto generation = source;
        generation.add_features(FeatureMayAlwaysSet::make_always(
            context->model.attach_to_sources(root)));
        auto port = AccessPath(root, path);
        LOG_OR_DUMP(
            context,
            4,
            "Inferred generation for port {}: {}",
            port,
            generation);
        context->model.add_generations(
            std::move(port), Taint{std::move(generation)});
      }

      if (source.is_artificial_sources()) {
        for (const auto& artificial_source : source) {
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
                context,
                4,
                "Inferred propagation {} to {}",
                propagation,
                output);
            context->model.add_propagation(
                std::move(propagation), std::move(output));
          }
        }
      }
    }
  }
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
  return_sinks.map([position](Taint& sinks) {
    sinks = sinks.attach_position(position, /* features */ {});
  });

  for (auto register_id : instruction->srcs()) {
    infer_output_taint(
        context, Root(Root::Kind::Return), environment->read(register_id));

    for (const auto& [path, sinks] : return_sinks.elements()) {
      Taint sources = environment->read(register_id, path).collapse();
      check_flows(context, sources, sinks);
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
