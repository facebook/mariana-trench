/*
 * Copyright (c) Facebook, Inc. and its affiliates.
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

using FulfilledPartialKindMap =
    std::unordered_map<const PartialKind*, FeatureMayAlwaysSet>;

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

  auto model = context->model_at_callsite(callee.call_target, position);
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

      taint.add_inferred_features_and_local_position(
          features,
          context->positions.get(callee.position, input, instruction));

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
    FrameSet source,
    FrameSet sink,
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
    const FrameSet& source,
    const FrameSet& sink,
    FulfilledPartialKindMap& fulfilled_partial_sinks,
    const MultiSourceMultiSinkRule* rule,
    const Position* position,
    const FeatureMayAlwaysSet& extra_features) {
  const auto* partial_sink = sink.kind()->as<PartialKind>();
  mt_assert(partial_sink != nullptr);

  // Check if the partial counterpart for this sink has been fulfilled.
  auto counterpart = std::find_if(
      fulfilled_partial_sinks.begin(),
      fulfilled_partial_sinks.end(),
      [partial_sink](const auto& other_sink) {
        return other_sink.first->is_counterpart(partial_sink);
      });

  // Features found by this branch of the multi-source-sink flow. Should be
  // reported as part of the final issue discovered.
  auto features = source.features_joined();
  features.add(sink.features_joined());

  if (counterpart != fulfilled_partial_sinks.end()) {
    // If both partial sinks for the callsite have been fulfilled, the rule
    // is satisfied. Make this a triggered sink and create the issue. Make
    // sure to include the features from the counterpart flow.
    auto issue_sink =
        sink.with_kind(context->kinds.get_triggered(partial_sink));
    issue_sink.add_inferred_features(counterpart->second);
    issue_sink.add_inferred_features(features);
    create_issue(context, source, issue_sink, rule, position, extra_features);
    // Issue was found. No need to track the counterpart nor itself.
    fulfilled_partial_sinks.erase(counterpart->first);
  } else {
    fulfilled_partial_sinks.emplace(partial_sink, features);
    LOG_OR_DUMP(
        context,
        4,
        "Found source kind {} flowing into partial sink: {}",
        *source.kind(),
        *partial_sink);
  }
}

const TriggeredPartialKind* MT_NULLABLE transform_partial_sinks(
    MethodContext* context,
    const FulfilledPartialKindMap& fulfilled_partial_sinks,
    const PartialKind* sink_kind) {
  for (const auto& [fulfilled_kind, _features] : fulfilled_partial_sinks) {
    if (fulfilled_kind->is_counterpart(sink_kind)) {
      // The counterpart sink was triggered when a source was found to flow into
      // it. Make this a triggered sink. This will be propagated.
      return context->kinds.get_triggered(sink_kind);
    }
  }

  // If the partial sink does not have a corresponding triggered
  // sink it is dropped (not propagated).
  return nullptr;
}

void create_sinks(
    MethodContext* context,
    const Taint& sources,
    const Taint& sinks,
    const FeatureMayAlwaysSet& extra_features = {},
    const FulfilledPartialKindMap& fulfilled_partial_sinks = {}) {
  if (sources.is_bottom() || sinks.is_bottom()) {
    return;
  }

  for (const auto& source : sources) {
    if (!source.is_artificial_sources()) {
      continue;
    }
    for (const auto& artificial_source : source) {
      auto features = extra_features;
      features.add_always(context->model.attach_to_sinks(
          artificial_source.callee_port().root()));
      features.add(artificial_source.features());

      // TODO(T66517244): This needs to be a transform_map_kind or something to
      // copy breadcrumbs when a kind transformation happens. We may also want
      // to include data about the other counterpart in the Frame so that the
      // issue created will be searchable by both this source/sink and its
      // counterpart.
      auto new_sinks = sinks.transform_kind(
          [context, &fulfilled_partial_sinks](
              const Kind* sink_kind) -> const Kind* MT_NULLABLE {
            const auto* partial_sink = sink_kind->as<PartialKind>();
            if (!partial_sink) {
              // No transformation. Keep sink as it is.
              return sink_kind;
            }
            return transform_partial_sinks(
                context, fulfilled_partial_sinks, partial_sink);
          });
      new_sinks.add_inferred_features(features);
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
    FulfilledPartialKindMap* MT_NULLABLE fulfilled_partial_sinks) {
  if (sources.is_bottom() || sinks.is_bottom()) {
    return;
  }

  for (const auto& source : sources) {
    if (source.is_artificial_sources()) {
      continue;
    }

    for (const auto& sink : sinks) {
      // Check if this satisfies any rule. If so, create the issue.
      const auto& rules = context->rules.rules(source.kind(), sink.kind());
      for (const auto* rule : rules) {
        create_issue(context, source, sink, rule, position, extra_features);
      }

      // Check if this satisfies any partial (multi-source/sink) rule.
      if (fulfilled_partial_sinks) {
        const auto* MT_NULLABLE partial_sink = sink.kind()->as<PartialKind>();
        if (partial_sink) {
          const auto& partial_rules =
              context->rules.partial_rules(source.kind(), partial_sink);
          for (const auto* partial_rule : partial_rules) {
            check_multi_source_multi_sink_rules(
                context,
                source,
                sink,
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
    const std::vector<Register>& instruction_sources,
    const Callee& callee,
    const FeatureMayAlwaysSet& extra_features = {}) {
  LOG_OR_DUMP(
      context,
      4,
      "Processing sinks for call to `{}`",
      show(callee.method_reference));

  FulfilledPartialKindMap fulfilled_partial_sinks;
  std::vector<std::tuple<AccessPath, Taint, const Taint&>> port_sources_sinks;

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
      /* inferred features */ {},
      /* user features */ {},
      /* via_type_of_ports */ {},
      /* local_positions */ {})};
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
        artificial_callee.register_parameters,
        get_callee(context, environment, artificial_callee),
        FeatureMayAlwaysSet::make_always(artificial_callee.features));
  }
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
    // Assume the method call returns a new memory location,
    // that does not alias with anything.
    memory_location = context->memory_factory.make_location(instruction);
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

  auto* position = context->positions.get(
      context->method(),
      environment->last_position(),
      Root(Root::Kind::Return),
      instruction);
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
    for (const auto& source : sources) {
      if (!source.is_artificial_sources()) {
        auto generation = source;
        generation.add_inferred_features(FeatureMayAlwaysSet::make_always(
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
  if (!memory_locations.is_value() || memory_locations.size() != 1) {
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
