/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/CallGraph.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/ForwardTaintTransfer.h>
#include <mariana-trench/FulfilledPartialKindState.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/OriginFactory.h>
#include <mariana-trench/PartialKind.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/SourceSinkWithExploitabilityRule.h>
#include <mariana-trench/TransferCall.h>
#include <mariana-trench/TransformOperations.h>
#include <mariana-trench/TriggeredPartialKind.h>

namespace marianatrench {

bool ForwardTaintTransfer::analyze_default(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  log_instruction(context, instruction);

  if (instruction->has_dest() || instruction->has_move_result_any()) {
    auto* memory_location = context->memory_factory.make_location(instruction);
    LOG_OR_DUMP(context, 4, "Tainting {} with {{}}", show(memory_location));
    environment->write(
        memory_location, TaintTree::bottom(), UpdateKind::Strong);
  }

  return false;
}

bool ForwardTaintTransfer::analyze_check_cast(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  auto taint = environment->read(
      aliasing.register_memory_locations(instruction->src(0)));

  // Add via-cast feature as configured by the program options.
  auto allowed_features = context->options.allow_via_cast_features();
  if (context->options.emit_all_via_cast_features() ||
      std::find(
          allowed_features.begin(),
          allowed_features.end(),
          instruction->get_type()->str()) != allowed_features.end()) {
    auto features = FeatureMayAlwaysSet::make_always(
        {context->feature_factory.get_via_cast_feature(
            instruction->get_type())});
    taint.add_locally_inferred_features(features);
  }

  auto* memory_location = aliasing.result_memory_location();
  LOG_OR_DUMP(
      context,
      4,
      "Tainting result register at {} with {}",
      show(memory_location),
      taint);
  environment->write(memory_location, taint, UpdateKind::Strong);

  return false;
}

bool ForwardTaintTransfer::analyze_iget(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  log_instruction(context, instruction);

  const auto field_target =
      context->call_graph.resolved_field_access(context->method(), instruction);
  if (!field_target) {
    WARNING_OR_DUMP(
        context,
        3,
        "Unable to resolve access of instance field {}",
        show(instruction->get_field()));
  } else {
    const auto& aliasing = context->aliasing.get(instruction);
    auto field_sources =
        context->field_sources_at_callsite(*field_target, aliasing);
    if (!field_sources.is_bottom()) {
      auto memory_locations = aliasing.result_memory_locations();
      LOG_OR_DUMP(
          context,
          4,
          "Tainting register {} at {} with {}",
          k_result_register,
          memory_locations,
          field_sources);
      environment->write(memory_locations, field_sources, UpdateKind::Weak);
    }
  }

  return false;
}

bool ForwardTaintTransfer::analyze_sget(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  const auto field_target =
      context->call_graph.resolved_field_access(context->method(), instruction);
  if (!field_target) {
    WARNING_OR_DUMP(
        context,
        3,
        "Unable to resolve access of static field {}",
        show(instruction->get_field()));
  } else {
    auto field_sources =
        context->field_sources_at_callsite(*field_target, aliasing);
    auto* memory_location = aliasing.result_memory_location();
    LOG_OR_DUMP(
        context,
        4,
        "Tainting register {} at {} with {}",
        k_result_register,
        show(memory_location),
        field_sources);
    environment->write(
        memory_location, TaintTree(field_sources), UpdateKind::Strong);
  }

  return false;
}

namespace {

void apply_generations(
    MethodContext* context,
    const InstructionAliasResults& aliasing,
    ForwardTaintEnvironment* environment,
    const IRInstruction* instruction,
    const CalleeModel& callee,
    TaintTree& result_taint) {
  LOG_OR_DUMP(
      context,
      4,
      "Processing generations for call to `{}`",
      show(callee.method_reference));

  for (const auto& [root, generations] : callee.model.generations().roots()) {
    switch (root.kind()) {
      case Root::Kind::Return: {
        LOG_OR_DUMP(context, 4, "Tainting invoke result with {}", generations);
        result_taint.join_with(generations);
        break;
      }
      case Root::Kind::Argument: {
        auto parameter_position = root.parameter_position();
        auto register_id = instruction->src(parameter_position);
        auto memory_locations = aliasing.register_memory_locations(register_id);
        LOG_OR_DUMP(
            context,
            4,
            "Tainting register {} at {} with {}",
            register_id,
            memory_locations,
            generations);
        environment->write(memory_locations, generations, UpdateKind::Weak);
        break;
      }
      default:
        mt_unreachable();
    }
  }
}

void apply_add_features_to_arguments(
    MethodContext* context,
    const InstructionAliasResults& aliasing,
    const ForwardTaintEnvironment* previous_environment,
    ForwardTaintEnvironment* new_environment,
    const IRInstruction* instruction,
    const CalleeModel& callee) {
  if (!callee.model.add_via_obscure_feature() &&
      !callee.model.has_add_features_to_arguments()) {
    return;
  }

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
      features.add_always(context->feature_factory.get("via-obscure"));
    }

    if (features.empty()) {
      continue;
    }

    auto register_id = instruction->src(parameter_position);
    auto memory_locations = aliasing.register_memory_locations(register_id);
    for (auto* memory_location : memory_locations.elements()) {
      auto taint = previous_environment->read(memory_location);
      taint.add_locally_inferred_features_and_local_position(
          features, position);
      // This is using a strong update, since a weak update would turn
      // the always-features we want to add into may-features.
      new_environment->write(
          memory_location, std::move(taint), UpdateKind::Strong);
    }
  }
}

void apply_propagations(
    MethodContext* context,
    const InstructionAliasResults& aliasing,
    const ForwardTaintEnvironment* previous_environment,
    ForwardTaintEnvironment* new_environment,
    const IRInstruction* instruction,
    const CalleeModel& callee,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    TaintTree& result_taint) {
  LOG_OR_DUMP(
      context,
      4,
      "Processing propagations for call to `{}`",
      show(callee.method_reference));

  for (const auto& binding : callee.model.propagations().elements()) {
    const AccessPath& input_path = binding.first;
    const Taint& propagations = binding.second;
    LOG_OR_DUMP(context, 4, "Processing propagations from {}", input_path);
    if (!input_path.root().is_argument()) {
      WARNING_OR_DUMP(
          context,
          2,
          "Ignoring propagation with a return input: {}",
          input_path);
      continue;
    }

    auto input_parameter_position = input_path.root().parameter_position();
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
    auto input_taint_tree = previous_environment->read(
        aliasing.register_memory_locations(input_register_id),
        input_path.path().resolve(source_constant_arguments));

    if (input_taint_tree.is_bottom() &&
        !callee.model.strong_write_on_propagation()) {
      continue;
    }

    auto position =
        context->positions.get(callee.position, input_path.root(), instruction);

    propagations.visit_frames([&aliasing,
                               &callee,
                               &input_path,
                               &input_taint_tree,
                               &propagations,
                               &result_taint,
                               &source_constant_arguments,
                               context,
                               instruction,
                               new_environment,
                               position](
                                  const CallInfo& call_info,
                                  const Frame& propagation) {
      LOG_OR_DUMP(
          context,
          4,
          "Processing propagation from {} to {}",
          input_path,
          propagation);

      const PropagationKind* propagation_kind = propagation.propagation_kind();
      auto transformed_taint_tree = transforms::apply_propagation(
          context,
          call_info,
          propagation,
          input_taint_tree,
          transforms::TransformDirection::Forward);

      auto output_root = propagation_kind->root();
      FeatureMayAlwaysSet features = FeatureMayAlwaysSet::make_always(
          callee.model.add_features_to_arguments(output_root));
      features.add(propagation.features());
      features.add(propagations.locally_inferred_features(call_info));
      features.add_always(
          callee.model.add_features_to_arguments(input_path.root()));

      transformed_taint_tree.add_locally_inferred_features_and_local_position(
          features, position);

      for (const auto& [output_path, collapse_depth] :
           propagation.output_paths().elements()) {
        auto output_taint_tree = transformed_taint_tree;

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
        if (collapse_depth.should_collapse() &&
            !callee.model.no_collapse_on_propagation()) {
          LOG_OR_DUMP(
              context,
              4,
              "Collapsing taint tree {} to depth {}",
              output_taint_tree,
              collapse_depth.value());
          output_taint_tree.collapse_deeper_than(
              /* height */ collapse_depth.value(),
              FeatureMayAlwaysSet{context->feature_factory
                                      .get_propagation_broadening_feature()});
        }

        auto output_path_resolved =
            output_path.resolve(source_constant_arguments);

        switch (output_root.kind()) {
          case Root::Kind::Return: {
            LOG_OR_DUMP(
                context,
                4,
                "Tainting invoke result path {} with {}",
                output_path_resolved,
                output_taint_tree);
            result_taint.write(
                output_path_resolved,
                std::move(output_taint_tree),
                UpdateKind::Weak);
            break;
          }
          case Root::Kind::Argument: {
            auto output_parameter_position = output_root.parameter_position();
            auto output_register_id =
                instruction->src(output_parameter_position);
            auto memory_locations =
                aliasing.register_memory_locations(output_register_id);
            LOG_OR_DUMP(
                context,
                4,
                "Tainting register {} at {} path {} with {}",
                output_register_id,
                memory_locations,
                output_path_resolved,
                output_taint_tree);
            new_environment->write(
                memory_locations,
                output_path_resolved,
                std::move(output_taint_tree),
                callee.model.strong_write_on_propagation() ? UpdateKind::Strong
                                                           : UpdateKind::Weak);
            break;
          }
          default:
            mt_unreachable();
        }
      }
    });
  }
}

void apply_inline_setter(
    MethodContext* context,
    const SetterInlineMemoryLocations& setter,
    const ForwardTaintEnvironment* previous_environment,
    ForwardTaintEnvironment* environment,
    TaintTree& result_taint) {
  auto taint = previous_environment->read(setter.value);
  taint.add_local_position(setter.position);
  LOG_OR_DUMP(context, 4, "Tainting {} with {}", show(setter.target), taint);
  environment->write(setter.target, taint, UpdateKind::Strong);

  result_taint = TaintTree::bottom();
}

void create_issue(
    MethodContext* context,
    Taint source,
    Taint sink,
    const Rule* rule,
    const Position* position,
    TextualOrderIndex sink_index,
    std::string_view callee,
    const FeatureMayAlwaysSet& extra_features) {
  // Skip creating issue if there are parameter type overrides.
  // The issue should be found in the copy of the Method that does not have
  // parameter type overrides.
  if (!context->method()->parameter_type_overrides().empty()) {
    LOG_OR_DUMP(
        context,
        4,
        "Skip creating issue for method with parameter type overrides.");
    return;
  }

  std::unordered_set<const Kind*> kinds;
  source.visit_frames([&kinds](const CallInfo&, const Frame& source_frame) {
    kinds.emplace(source_frame.kind());
  });
  sink.visit_frames([&kinds](const CallInfo&, const Frame& sink_frame) {
    kinds.emplace(sink_frame.kind());
  });

  source.add_locally_inferred_features(context->class_properties.issue_features(
      context->method(), kinds, context->heuristics));

  sink.add_locally_inferred_features(extra_features);
  auto issue = Issue(
      Taint{std::move(source)},
      Taint{std::move(sink)},
      rule,
      callee,
      sink_index,
      position);
  LOG_OR_DUMP(context, 4, "Found issue: {}", issue);
  context->new_model.add_issue(std::move(issue));
}

/**
 * Checks if the given sources/sinks fulfill any rule. If so, create an issue.
 */
void check_source_sink_rules(
    MethodContext* context,
    const Kind* source_kind,
    const Taint& source_taint,
    const Kind* sink_kind,
    const Taint& sink_taint,
    const Position* position,
    TextualOrderIndex sink_index,
    std::string_view callee,
    const FeatureMayAlwaysSet& extra_features) {
  const auto& rules = context->rules.rules(source_kind, sink_kind);
  for (const auto* rule : rules) {
    create_issue(
        context,
        source_taint,
        sink_taint,
        rule,
        position,
        sink_index,
        callee,
        extra_features);
  }
}

/**
 * Check if given exploitability source and source-as-transform sink fulfills
 * any exploitability rule. If the rule is fulfilled, an issue is created. else
 * the source-as-transform sink is stored in FulfilledExploitabilityState for
 * backwards analysis to use.
 */
void check_fulfilled_exploitability_rules(
    MethodContext* context,
    const IRInstruction* instruction,
    const Kind* exploitability_source_kind,
    const Taint& exploitability_source_taint,
    const TransformKind* source_as_transform_sink_kind,
    Taint& source_as_transform_sink_taint,
    const Position* position,
    TextualOrderIndex sink_index,
    std::string_view callee,
    const FeatureMayAlwaysSet& extra_features) {
  mt_assert(source_as_transform_sink_kind->has_source_as_transform());

  const auto& fulfilled_rules = context->rules.fulfilled_exploitability_rules(
      exploitability_source_kind, source_as_transform_sink_kind);

  if (fulfilled_rules.empty()) {
    // If an exploitability rule cannot be fulfilled,
    // pass it to backwards analysis to propagate the source-as-transform sinks.
    context->partially_fulfilled_exploitability_state
        .add_source_as_transform_sinks(
            instruction, source_as_transform_sink_taint);
    return;
  }

  auto exploitability_origins =
      source_as_transform_sink_taint.exploitability_origins();
  mt_assert(!exploitability_origins.is_bottom());

  source_as_transform_sink_taint.transform_frames(
      [](Frame frame) { return frame.without_exploitability_origins(); });

  // Create an issue per exploitability-origin for fulfilled exploitability
  // rules
  for (const auto* rule : fulfilled_rules) {
    LOG_OR_DUMP(
        context,
        4,
        "Fulfilled exploitability rule: {}. Creating issue with: Source: {}, Sink: {}",
        rule->code(),
        exploitability_source_kind->to_trace_string(),
        source_as_transform_sink_kind->to_trace_string());

    for (const auto* origin : exploitability_origins) {
      const auto* exploitability_origin = origin->as<ExploitabilityOrigin>();
      mt_assert(exploitability_origin != nullptr);

      // Add exploitability_origin's callee as a feature.
      auto extra_features_copy = extra_features;
      extra_features_copy.add_always(
          context->feature_factory.get_exploitability_origin_feature(
              exploitability_origin));

      create_issue(
          context,
          exploitability_source_taint,
          source_as_transform_sink_taint,
          rule,
          position,
          sink_index,
          exploitability_origin->issue_handle_callee(),
          extra_features_copy);
    }
  }
}

/**
 * Check if given source and sink kinds partially fulfills any exploitability
 * rule. If partially fulfilled, computes the corresponding source-as-transform
 * sink taint and checks if the materialized source-as-transform sink can
 * trivially fulfill any exploitability rule or if we need to infer the
 * source-as-transform sink on the call-effect exploitability port of the
 * root-callable by checking against exploitability sources of the caller
 * itself.
 */
void check_partially_fulfilled_exploitability_rules(
    MethodContext* context,
    const IRInstruction* instruction,
    const Kind* source_kind,
    const Taint& source_taint,
    const Kind* sink_kind,
    const Taint& sink_taint,
    const Position* position,
    TextualOrderIndex sink_index,
    std::string_view callee,
    const FeatureMayAlwaysSet& extra_features) {
  const auto& partially_fulfilled_rules =
      context->rules.partially_fulfilled_exploitability_rules(
          source_kind, sink_kind);

  if (partially_fulfilled_rules.empty()) {
    // No "source" to "sink" flow found.
    return;
  }

  const auto* source_as_transform =
      context->transforms_factory.get_source_as_transform(source_kind);
  mt_assert(source_as_transform != nullptr);

  auto transformed_sink_with_extra_trace =
      transforms::apply_source_as_transform_to_sink(
          context, source_taint, source_as_transform, sink_taint, callee);

  // Collapse taint tree as exploitability port does not use paths.
  auto exploitability_sources =
      context->previous_model.call_effect_sources()
          .read(Root{Root::Kind::CallEffectExploitability})
          .collapse();

  if (exploitability_sources.is_bottom()) {
    // If an exploitability rule cannot be fulfilled,
    // pass it to backwards analysis to propagate the source-as-transform sinks.
    context->partially_fulfilled_exploitability_state
        .add_source_as_transform_sinks(
            instruction, transformed_sink_with_extra_trace);
    return;
  }

  // Add the position of the caller to call effect sources.
  auto* caller_position = context->positions.get(context->method());

  // Check for trivially fulfilled case.
  for (auto& [source_kind, source_taint] :
       exploitability_sources.partition_by_kind()) {
    for (auto& [sink_kind, sink_taint] :
         transformed_sink_with_extra_trace.partition_by_kind()) {
      check_fulfilled_exploitability_rules(
          context,
          instruction,
          /* exploitability_source_kind */ source_kind,
          /* exploitability_source_taint */
          source_taint.attach_position(caller_position),
          /* source_as_transform_sink_kind */ sink_kind->as<TransformKind>(),
          /* source_as_transform_sink_taint */ sink_taint,
          position,
          sink_index,
          callee,
          extra_features);
    }
  }
}

/**
 * Check if the sources/sinks fulfill (or partially fulfill) any
 * exploitability rules.
 */
void check_exploitability_rules(
    MethodContext* context,
    const IRInstruction* instruction,
    const Kind* source_kind,
    const Taint& source_taint,
    const Kind* sink_kind,
    Taint& sink_taint,
    const Position* position,
    TextualOrderIndex sink_index,
    std::string_view callee,
    const FeatureMayAlwaysSet& extra_features) {
  const TransformKind* source_as_transform_sink = nullptr;
  if (const auto* transform_kind = sink_kind->as<TransformKind>()) {
    if (transform_kind->has_source_as_transform()) {
      source_as_transform_sink = transform_kind;
    }
  }

  if (source_as_transform_sink == nullptr) {
    check_partially_fulfilled_exploitability_rules(
        context,
        instruction,
        source_kind,
        source_taint,
        sink_kind,
        sink_taint,
        position,
        sink_index,
        callee,
        extra_features);
  } else {
    check_fulfilled_exploitability_rules(
        context,
        instruction,
        source_kind,
        source_taint,
        source_as_transform_sink,
        sink_taint,
        position,
        sink_index,
        callee,
        extra_features);
  }
}

/**
 * If fulfilled_partial_sinks is non-null, checks if a source is flows into a
 * partial sink for a multi source rule.
 *
 * If a partial rule is fulfilled, this converts a partial
 * sink to a triggered sink and accumulates this list of triggered sinks. How
 * these sinks should be handled depends on what happens at other sinks/ports
 * within the same callsite/invoke. The caller MUST accumulate triggered sinks
 * at the callsite then pass the results to the backward analysis.
 */
void check_multi_source_multi_sink_rules(
    MethodContext* context,
    const Kind* source_kind,
    const Taint& source_taint,
    const Kind* sink_kind,
    const Taint& sink_taint,
    const Position* position,
    TextualOrderIndex sink_index,
    std::string_view callee,
    const FeatureMayAlwaysSet& extra_features,
    FulfilledPartialKindState* MT_NULLABLE fulfilled_partial_sinks) {
  if (fulfilled_partial_sinks == nullptr) {
    return;
  }

  const auto* MT_NULLABLE partial_sink = sink_kind->as<PartialKind>();
  if (partial_sink == nullptr) {
    return;
  }

  const auto& partial_rules =
      context->rules.partial_rules(source_kind, partial_sink);
  for (const auto* partial_rule : partial_rules) {
    // Features found by this branch of the multi-source-sink flow. Should be
    // reported as part of the final issue discovered.
    auto features = source_taint.features_joined();
    features.add(sink_taint.features_joined());

    auto issue_sink_frame = fulfilled_partial_sinks->fulfill_kind(
        partial_sink,
        partial_rule,
        features,
        sink_taint,
        context->kind_factory);

    if (issue_sink_frame) {
      create_issue(
          context,
          source_taint,
          *issue_sink_frame,
          partial_rule,
          position,
          sink_index,
          callee,
          extra_features);
    } else {
      LOG_OR_DUMP(
          context,
          4,
          "Found source kind: {} flowing into partial sink: {}, rule code: {}",
          *source_kind,
          *sink_kind,
          partial_rule->code());
    }
  }
}

/**
 * Checks if the given sources/sinks fulfill any of the rules:
 * - Source to Sink rules: Create an issue.
 * - Exploitability rules: Infer source-as-transform sinks and/or create an
 * issue.
 * - Multi source/sink rules: Create triggered counter parts and/or create an
 * issue.
 */
void check_sources_sinks_flows(
    MethodContext* context,
    const IRInstruction* instruction,
    const Taint& sources,
    const Taint& sinks,
    const Position* position,
    TextualOrderIndex sink_index,
    std::string_view callee,
    const FeatureMayAlwaysSet& extra_features,
    FulfilledPartialKindState* MT_NULLABLE fulfilled_partial_sinks) {
  if (sources.is_bottom() || sinks.is_bottom()) {
    return;
  }

  // Note: We use a sorted partition for source kinds. Deterministic
  // iteration order is required to produce stable results for multi-source
  // rules where both the source kinds are found within the context of the
  // current method.
  auto sources_by_kind = sources.sorted_partition_by_kind();
  auto sinks_by_kind = sinks.partition_by_kind();
  for (auto& [source_kind, source_taint] : sources_by_kind) {
    for (auto& [sink_kind, sink_taint] : sinks_by_kind) {
      source_taint.intersect_intervals_with(sink_taint);
      sink_taint.intersect_intervals_with(source_taint);
      if (source_taint.is_bottom() || sink_taint.is_bottom()) {
        // Intervals do not intersect, flow is not possible.
        continue;
      }

      // Check if this satisfies any source-sink rule. If so, create the issue.
      check_source_sink_rules(
          context,
          source_kind,
          source_taint,
          sink_kind,
          sink_taint,
          position,
          sink_index,
          callee,
          extra_features);

      // Check if this satisfies any exploitability rule.
      check_exploitability_rules(
          context,
          instruction,
          source_kind,
          source_taint,
          sink_kind,
          sink_taint,
          position,
          sink_index,
          callee,
          extra_features);

      // Check if this satisfies any partial (multi-source/sink) rule.
      check_multi_source_multi_sink_rules(
          context,
          source_kind,
          source_taint,
          sink_kind,
          sink_taint,
          position,
          sink_index,
          callee,
          extra_features,
          fulfilled_partial_sinks);
    }
  }
}

void check_call_flows(
    MethodContext* context,
    const IRInstruction* instruction,
    const InstructionAliasResults& aliasing,
    const ForwardTaintEnvironment* environment,
    const std::function<std::optional<Register>(Root)>& get_register,
    const CalleeModel& callee,
    const TaintAccessPathTree& sinks,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    const FeatureMayAlwaysSet& extra_features,
    FulfilledPartialKindState* MT_NULLABLE fulfilled_partial_sinks) {
  LOG_OR_DUMP(
      context,
      4,
      "Processing sinks for call to `{}`",
      show(callee.method_reference));

  for (const auto& [port, sinks] : sinks.elements()) {
    auto register_id = get_register(port.root());
    if (!register_id) {
      continue;
    }

    Taint sources =
        environment
            ->read(
                aliasing.register_memory_locations(*register_id),
                port.path().resolve(source_constant_arguments))
            .collapse(FeatureMayAlwaysSet{
                context->feature_factory.get_issue_broadening_feature()});
    check_sources_sinks_flows(
        context,
        instruction,
        sources,
        sinks,
        callee.position,
        /* sink_index */ callee.call_index,
        /* callee */ callee.resolved_base_method
            ? callee.resolved_base_method->show()
            : std::string(k_unresolved_callee),
        extra_features,
        fulfilled_partial_sinks);
  }
}

void check_call_flows(
    MethodContext* context,
    const IRInstruction* instruction,
    const InstructionAliasResults& aliasing,
    const ForwardTaintEnvironment* environment,
    const std::vector<Register>& instruction_sources,
    const CalleeModel& callee,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    const FeatureMayAlwaysSet& extra_features,
    FulfilledPartialKindState* MT_NULLABLE fulfilled_partial_sinks) {
  check_call_flows(
      context,
      instruction,
      aliasing,
      environment,
      /* get_register */
      [&instruction_sources](
          Root parameter_position) -> std::optional<Register> {
        if (!parameter_position.is_argument()) {
          return std::nullopt;
        }
        if (parameter_position.parameter_position() >=
            instruction_sources.size()) {
          return std::nullopt;
        }

        return instruction_sources.at(parameter_position.parameter_position());
      },
      callee,
      callee.model.sinks(),
      source_constant_arguments,
      extra_features,
      fulfilled_partial_sinks);
}

void check_flows_to_array_allocation(
    MethodContext* context,
    const InstructionAliasResults& aliasing,
    ForwardTaintEnvironment* environment,
    const IRInstruction* instruction) {
  if (!context->artificial_methods.array_allocation_kind_used()) {
    return;
  }

  auto* array_allocation_method = context->methods.get(
      context->artificial_methods.array_allocation_method());
  auto* position =
      context->positions.get(context->method(), aliasing.position());
  const auto* port = context->access_path_factory.get(
      AccessPath(Root(Root::Kind::Argument, 0)));
  auto array_allocation_sink = Taint{TaintConfig(
      /* kind */ context->artificial_methods.array_allocation_kind(),
      /* callee_port */ port,
      /* callee */ nullptr,
      /* call_kind */ CallKind::origin(),
      /* call_position */ position,
      /* class_interval_context */ CallClassIntervalContext(),
      /* distance */ 1,
      /* origins */
      OriginSet{
          context->origin_factory.method_origin(array_allocation_method, port)},
      /* inferred features */ {},
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* canonical_names */ {},
      /* output_paths */ {},
      /* local_positions */ {},
      /* extra_traces */ {})};
  auto array_allocation_index = context->call_graph.array_allocation_index(
      context->method(), instruction);
  for (std::size_t parameter_position = 0,
                   number_parameters = instruction->srcs_size();
       parameter_position < number_parameters;
       parameter_position++) {
    auto register_id = instruction->src(parameter_position);
    Taint sources =
        environment->read(aliasing.register_memory_locations(register_id))
            .collapse(FeatureMayAlwaysSet{
                context->feature_factory.get_issue_broadening_feature()});
    // Fulfilled partial sinks ignored. No partial sinks for array allocation.
    check_sources_sinks_flows(
        context,
        instruction,
        sources,
        array_allocation_sink,
        position,
        /* sink_index */ array_allocation_index,
        /* callee */ array_allocation_method->show(),
        /* extra_features */ {},
        /* fulfilled_partial_sinks */ nullptr);
  }
}

void check_artificial_calls_flows(
    MethodContext* context,
    const InstructionAliasResults& aliasing,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment,
    const std::vector<std::optional<std::string>>& source_constant_arguments =
        {}) {
  const auto& artificial_callees =
      context->call_graph.artificial_callees(context->method(), instruction);

  for (const auto& artificial_callee : artificial_callees) {
    auto callee = get_callee(context, artificial_callee, aliasing.position());
    auto get_register =
        [&artificial_callee](
            Root parameter_position) -> std::optional<Register> {
      auto found = artificial_callee.root_registers.find(parameter_position);
      if (found == artificial_callee.root_registers.end()) {
        return std::nullopt;
      }

      return found->second;
    };
    auto extra_features =
        FeatureMayAlwaysSet::make_always(artificial_callee.features);

    FulfilledPartialKindState fulfilled_partial_sinks;
    check_call_flows(
        context,
        instruction,
        aliasing,
        environment,
        get_register,
        callee,
        callee.model.sinks(),
        source_constant_arguments,
        extra_features,
        &fulfilled_partial_sinks);

    check_call_flows(
        context,
        instruction,
        aliasing,
        environment,
        get_register,
        callee,
        callee.model.call_effect_sinks(),
        source_constant_arguments,
        extra_features,
        &fulfilled_partial_sinks);

    context->fulfilled_partial_sinks.store_artificial_call(
        &artificial_callee, std::move(fulfilled_partial_sinks));
  }
}

void check_call_effect_flows(
    MethodContext* context,
    const IRInstruction* instruction,
    const CalleeModel& callee) {
  const auto& callee_call_effect_sinks = callee.model.call_effect_sinks();
  if (callee_call_effect_sinks.is_bottom()) {
    return;
  }

  const auto& caller_call_effect_sources =
      context->previous_model.call_effect_sources();

  auto* caller_position = context->positions.get(context->method());
  for (const auto& [port, sinks] : callee_call_effect_sinks.elements()) {
    const auto& sources = caller_call_effect_sources.read(port);
    if (sources.is_bottom()) {
      if (port.root().is_call_chain_exploitability()) {
        // If an exploitability rule cannot be fulfilled,
        // pass it to backwards analysis to propagate it.
        context->partially_fulfilled_exploitability_state
            .add_source_as_transform_sinks(instruction, sinks);
      }
      continue;
    }

    for (const auto& [_, sources] : sources.elements()) {
      check_sources_sinks_flows(
          context,
          instruction,
          // Add the position of the caller to call effect sources.
          sources.attach_position(caller_position),
          sinks,
          callee.position,
          callee.call_index,
          /* sink_index */ callee.resolved_base_method
              ? callee.resolved_base_method->show()
              : std::string(k_unresolved_callee),
          /* extra features */ {},
          /* fulfilled partial sinks */ nullptr);
    }
  }
}

void check_artificial_call_effect_flows(
    MethodContext* context,
    const InstructionAliasResults& aliasing,
    const IRInstruction* instruction) {
  const auto& artificial_callees =
      context->call_graph.artificial_callees(context->method(), instruction);

  const auto* caller_position = context->positions.get(context->method());

  const auto& caller_call_effect_sources =
      context->previous_model.call_effect_sources();

  for (const auto& artificial_callee : artificial_callees) {
    auto callee = get_callee(context, artificial_callee, aliasing.position());
    const auto& callee_call_effect_sinks = callee.model.call_effect_sinks();

    if (callee_call_effect_sinks.is_bottom()) {
      continue;
    }

    for (const auto& [port, sinks] : callee_call_effect_sinks.elements()) {
      const auto& sources = caller_call_effect_sources.read(port);
      if (sources.is_bottom()) {
        if (port.root().is_call_chain_exploitability()) {
          // If an exploitability rule cannot be fulfilled, pass it to backwards
          // analysis to propagate it.
          context->partially_fulfilled_exploitability_state
              .add_source_as_transform_sinks(instruction, sinks);
        }
        continue;
      }

      for (const auto& [_, sources] : sources.elements()) {
        check_sources_sinks_flows(
            context,
            instruction,
            // Add the position of the caller to call effect sources.
            sources.attach_position(caller_position),
            sinks,
            callee.position,
            callee.call_index,
            /* sink_index */ callee.resolved_base_method
                ? callee.resolved_base_method->show()
                : std::string(k_unresolved_callee),
            /* extra features */ {},
            /* fulfilled partial sinks */ nullptr);
      }
    }
  }
}

} // namespace

bool ForwardTaintTransfer::analyze_invoke(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  auto source_constant_arguments = get_source_constant_arguments(
      aliasing.register_memory_locations_map(), instruction);
  auto callee = get_callee(
      context,
      instruction,
      aliasing.position(),
      get_source_register_types(context, instruction),
      source_constant_arguments,
      get_is_this_call(aliasing.register_memory_locations_map(), instruction));

  const ForwardTaintEnvironment previous_environment = *environment;

  FulfilledPartialKindState fulfilled_partial_sinks;
  check_call_flows(
      context,
      instruction,
      aliasing,
      &previous_environment,
      instruction->srcs_vec(),
      callee,
      source_constant_arguments,
      /* extra_features */ {},
      &fulfilled_partial_sinks);
  context->fulfilled_partial_sinks.store_call(
      instruction, std::move(fulfilled_partial_sinks));

  check_call_effect_flows(context, instruction, callee);

  TaintTree result_taint;
  apply_add_features_to_arguments(
      context,
      aliasing,
      &previous_environment,
      environment,
      instruction,
      callee);

  if (auto setter = try_inline_invoke_as_setter(
          context,
          aliasing.register_memory_locations_map(),
          instruction,
          callee);
      setter) {
    apply_inline_setter(
        context, *setter, &previous_environment, environment, result_taint);
  } else {
    apply_propagations(
        context,
        aliasing,
        &previous_environment,
        environment,
        instruction,
        callee,
        source_constant_arguments,
        result_taint);
  }

  apply_generations(
      context, aliasing, environment, instruction, callee, result_taint);

  if (callee.resolved_base_method &&
      callee.resolved_base_method->returns_void()) {
    // No result.
  } else if (
      try_inline_invoke_as_getter(
          context,
          aliasing.register_memory_locations_map(),
          instruction,
          callee) != nullptr) {
    // Since we are inlining the call, we should NOT write any taint.
    LOG_OR_DUMP(context, 4, "Inlining method call");
  } else {
    auto* memory_location = aliasing.result_memory_location();
    LOG_OR_DUMP(
        context, 4, "Tainting {} with {}", show(memory_location), result_taint);
    environment->write(memory_location, result_taint, UpdateKind::Weak);
  }

  check_artificial_calls_flows(
      context, aliasing, instruction, environment, source_constant_arguments);
  check_artificial_call_effect_flows(context, aliasing, instruction);

  return false;
}

namespace {

void check_flows_to_field_sink(
    MethodContext* context,
    const IRInstruction* instruction,
    const TaintTree& source_taint,
    const Position* position) {
  mt_assert(
      opcode::is_an_sput(instruction->opcode()) ||
      opcode::is_an_iput(instruction->opcode()));

  if (source_taint.is_bottom()) {
    return;
  }

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

  const auto& aliasing = context->aliasing.get(instruction);
  auto field_sinks = context->field_sinks_at_callsite(*field_target, aliasing);
  if (field_sinks.is_bottom()) {
    return;
  }

  for (const auto& [port, sources] : source_taint.elements()) {
    check_sources_sinks_flows(
        context,
        instruction,
        sources,
        field_sinks,
        position,
        /* sink_index */ field_target->field_sink_index,
        /* callee */ show(field_target->field),
        /* extra_features */ FeatureMayAlwaysSet(),
        /* fulfilled_partial_sinks */ nullptr);
  }
}

} // namespace

bool ForwardTaintTransfer::analyze_iput(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  auto taint = environment->read(
      aliasing.register_memory_locations(instruction->src(0)));
  auto* position = context->positions.get(
      context->method(),
      aliasing.position(),
      Root(Root::Kind::Return),
      instruction);
  taint.add_local_position(position);

  check_flows_to_field_sink(context, instruction, taint, position);

  // Store the taint in the memory location(s) representing the field
  auto* field_name = instruction->get_field()->get_name();
  auto target_memory_locations =
      aliasing.register_memory_locations(instruction->src(1));
  bool is_singleton = target_memory_locations.singleton() != nullptr;

  for (auto* memory_location : target_memory_locations.elements()) {
    auto* field_memory_location = memory_location->make_field(field_name);
    auto taint_copy = taint;
    add_field_features(context, taint_copy, field_memory_location);

    LOG_OR_DUMP(
        context,
        4,
        "Tainting {} with {} update kind: {}",
        show(field_memory_location),
        taint_copy,
        (is_singleton ? "Strong" : "Weak"));
    environment->write(
        field_memory_location,
        taint_copy,
        is_singleton ? UpdateKind::Strong : UpdateKind::Weak);
  }

  check_artificial_calls_flows(context, aliasing, instruction, environment);
  check_artificial_call_effect_flows(context, aliasing, instruction);

  return false;
}

bool ForwardTaintTransfer::analyze_sput(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  auto taint = environment->read(
      aliasing.register_memory_locations(instruction->src(0)));
  if (taint.is_bottom()) {
    return false;
  }
  auto* position = context->positions.get(
      context->method(),
      aliasing.position(),
      Root(Root::Kind::Return),
      instruction);
  taint.add_local_position(position);
  check_flows_to_field_sink(context, instruction, taint, position);
  return false;
}

bool ForwardTaintTransfer::analyze_load_param(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
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

  // Add parameter sources specified in model generators.
  auto root = Root(Root::Kind::Argument, parameter_memory_location->position());
  auto taint = context->previous_model.parameter_sources().read(root);

  // Add the position of the instruction to the parameter sources.
  auto* position = context->positions.get(context->method());
  taint.attach_position(position);

  LOG_OR_DUMP(context, 4, "Tainting {} with {}", show(memory_location), taint);
  environment->write(memory_location, std::move(taint), UpdateKind::Strong);

  return false;
}

bool ForwardTaintTransfer::analyze_move(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* /* environment */) {
  log_instruction(context, instruction);

  // This is a no-op for taint.
  return false;
}

bool ForwardTaintTransfer::analyze_move_result(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* /* environment */) {
  log_instruction(context, instruction);

  // This is a no-op for taint.
  return false;
}

bool ForwardTaintTransfer::analyze_aget(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* /* environment */) {
  log_instruction(context, instruction);

  // This is a no-op for taint.
  return false;
}

bool ForwardTaintTransfer::analyze_aput(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  auto taint = environment->read(
      aliasing.register_memory_locations(instruction->src(0)));

  auto features = FeatureMayAlwaysSet::make_always(
      {context->feature_factory.get("via-array")});
  auto* position = context->positions.get(
      context->method(),
      aliasing.position(),
      Root(Root::Kind::Return),
      instruction);
  taint.add_locally_inferred_features_and_local_position(features, position);

  // We use a single memory location for the array and its elements.
  auto memory_locations =
      aliasing.register_memory_locations(instruction->src(1));
  LOG_OR_DUMP(
      context,
      4,
      "Tainting register {} at {} with {}",
      instruction->src(1),
      memory_locations,
      taint);
  environment->write(memory_locations, taint, UpdateKind::Weak);

  return false;
}

bool ForwardTaintTransfer::analyze_new_array(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  check_flows_to_array_allocation(
      context, context->aliasing.get(instruction), environment, instruction);
  return analyze_default(context, instruction, environment);
}

bool ForwardTaintTransfer::analyze_filled_new_array(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  check_flows_to_array_allocation(
      context, context->aliasing.get(instruction), environment, instruction);

  const auto& aliasing = context->aliasing.get(instruction);

  auto features = FeatureMayAlwaysSet::make_always(
      {context->feature_factory.get("via-array")});
  auto* position = context->positions.get(
      context->method(),
      aliasing.position(),
      Root(Root::Kind::Return),
      instruction);

  mt_assert(instruction->srcs_size() >= 1);
  auto taint = environment->read(
      aliasing.register_memory_locations(instruction->src(0)));
  for (size_t i = 1; i < instruction->srcs_size(); ++i) {
    taint.join_with(environment->read(
        aliasing.register_memory_locations(instruction->src(i))));
  }

  taint.add_locally_inferred_features_and_local_position(features, position);

  // We use a single memory location for the array and its elements.
  auto* memory_location = aliasing.result_memory_location();
  LOG_OR_DUMP(context, 4, "Tainting {} with {}", show(memory_location), taint);
  environment->write(memory_location, taint, UpdateKind::Weak);

  return false;
}

namespace {

static bool analyze_numerical_operator(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  const auto& aliasing = context->aliasing.get(instruction);

  TaintTree taint;
  for (auto register_id : instruction->srcs()) {
    taint.join_with(
        environment->read(aliasing.register_memory_locations(register_id)));
  }

  auto features = FeatureMayAlwaysSet::make_always(
      {context->feature_factory.get("via-numerical-operator")});
  auto* position = context->positions.get(
      context->method(),
      aliasing.position(),
      Root(Root::Kind::Return),
      instruction);
  taint.add_locally_inferred_features_and_local_position(features, position);

  auto* memory_location = aliasing.result_memory_location();
  LOG_OR_DUMP(context, 4, "Tainting {} with {}", show(memory_location), taint);
  environment->write(memory_location, taint, UpdateKind::Strong);

  return false;
}

} // namespace

bool ForwardTaintTransfer::analyze_unop(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  return analyze_numerical_operator(context, instruction, environment);
}

bool ForwardTaintTransfer::analyze_binop(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  return analyze_numerical_operator(context, instruction, environment);
}

bool ForwardTaintTransfer::analyze_binop_lit(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  return analyze_numerical_operator(context, instruction, environment);
}

namespace {

// Infer generations for the output `taint` on port `root`.
void infer_output_taint(
    MethodContext* context,
    Root output_root,
    const TaintTree& taint) {
  for (const auto& [output_path, sources] : taint.elements()) {
    auto generation = sources;
    generation.add_locally_inferred_features(FeatureMayAlwaysSet::make_always(
        context->previous_model.attach_to_sources(output_root)));
    auto port = AccessPath(output_root, output_path);
    LOG_OR_DUMP(
        context, 4, "Inferred generation for port {}: {}", port, generation);
    context->new_model.add_inferred_generations(
        std::move(port),
        std::move(generation),
        /* widening_features */
        FeatureMayAlwaysSet{
            context->feature_factory.get_widen_broadening_feature()},
        context->heuristics);
  }
}

} // namespace

bool ForwardTaintTransfer::analyze_const_string(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  log_instruction(context, instruction);

  const std::string_view literal{instruction->get_string()->str()};
  const auto& aliasing = context->aliasing.get(instruction);

  auto sources = context->literal_sources_at_callsite(literal, aliasing);
  if (sources.empty()) {
    return false;
  }

  auto memory_locations = aliasing.result_memory_locations();
  LOG_OR_DUMP(
      context,
      4,
      "Tainting register {} at {} with {}",
      k_result_register,
      memory_locations,
      sources);

  environment->write(memory_locations, sources, UpdateKind::Strong);

  return false;
}

bool ForwardTaintTransfer::analyze_return(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardTaintEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs_size() <= 1);
  const auto& aliasing = context->aliasing.get(instruction);

  auto return_sinks =
      context->previous_model.sinks().read(Root(Root::Kind::Return));

  // Add the position of the instruction to the return sinks.
  auto* position =
      context->positions.get(context->method(), aliasing.position());
  return_sinks.attach_position(position);
  auto return_index =
      context->call_graph.return_index(context->method(), instruction);

  if (instruction->srcs_size() == 1) {
    auto register_id = instruction->src(0);
    auto memory_locations = aliasing.register_memory_locations(register_id);
    infer_output_taint(
        context, Root(Root::Kind::Return), environment->read(memory_locations));

    for (const auto& [path, sinks] : return_sinks.elements()) {
      Taint sources =
          environment->read(memory_locations, path)
              .collapse(FeatureMayAlwaysSet{
                  context->feature_factory.get_issue_broadening_feature()});
      // Fulfilled partial sinks are not expected to be produced here. Return
      // sinks are never partial.
      check_sources_sinks_flows(
          context,
          instruction,
          sources,
          sinks,
          position,
          /* sink_index */ return_index,
          /* callee */ std::string(k_return_callee),
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
