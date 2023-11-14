/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/MethodContext.h>

#include <Show.h>

#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Positions.h>

namespace marianatrench {

MethodContext::MethodContext(
    Context& context,
    const Registry& registry,
    const Model& previous_model,
    Model& new_model)
    : options(*context.options),
      artificial_methods(*context.artificial_methods),
      methods(*context.methods),
      fields(*context.fields),
      positions(*context.positions),
      types(*context.types),
      class_properties(*context.class_properties),
      class_intervals(*context.class_intervals),
      overrides(*context.overrides),
      call_graph(*context.call_graph),
      rules(*context.rules),
      dependencies(*context.dependencies),
      scheduler(*context.scheduler),
      kind_factory(*context.kind_factory),
      feature_factory(*context.feature_factory),
      registry(registry),
      transforms_factory(*context.transforms_factory),
      used_kinds(*context.used_kinds),
      access_path_factory(*context.access_path_factory),
      origin_factory(*context.origin_factory),
      memory_factory(previous_model.method()),
      previous_model(previous_model),
      new_model(new_model),
      context_(context),
      dump_(previous_model.method()->should_be_logged(options)) {}

Model MethodContext::model_at_callsite(
    const CallTarget& call_target,
    const Position* position,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    const CallClassIntervalContext& class_interval_context) const {
  auto* caller = method();

  LOG_OR_DUMP(
      this,
      5,
      "Getting model for {} call `{}`",
      (call_target.is_virtual() ? "virtual" : "static"),
      show(call_target.resolved_base_callee()));

  if (!call_target.resolved()) {
    return Model(
        /* method */ nullptr,
        context_,
        Model::Mode::SkipAnalysis | Model::Mode::AddViaObscureFeature |
            Model::Mode::TaintInTaintOut);
  }

  if (call_target.is_virtual()) {
    auto cached = callsite_model_cache_.find(CacheKey{call_target, position});
    if (cached != callsite_model_cache_.end()) {
      return cached->second;
    }
  }

  auto model = registry.get(call_target.resolved_base_callee())
                   .at_callsite(
                       caller,
                       position,
                       context_,
                       source_register_types,
                       source_constant_arguments,
                       class_interval_context);

  if (!call_target.is_virtual()) {
    return model;
  }

  if (model.no_join_virtual_overrides()) {
    LOG_OR_DUMP(
        this,
        5,
        "Not joining at call-site for method `{}`",
        show(call_target.resolved_base_callee()));
    return model;
  }

  LOG_OR_DUMP(
      this,
      5,
      "Initial model for `{}`: {}",
      show(call_target.resolved_base_callee()),
      model);

  for (const auto* override : call_target.overrides()) {
    auto override_model = registry.get(override).at_callsite(
        caller,
        position,
        context_,
        source_register_types,
        source_constant_arguments,
        class_interval_context);
    LOG_OR_DUMP(
        this,
        5,
        "Joining with model for `{}`: {}",
        show(override),
        override_model);
    model.join_with(override_model);
  }

  model.approximate(
      FeatureMayAlwaysSet{feature_factory.get_widen_broadening_feature()});

  callsite_model_cache_.emplace(CacheKey{call_target, position}, model);
  return model;
}

namespace {

Taint propagate_field_taint(
    const Taint& taint,
    const Position* call_position,
    const Options& options,
    Context& context) {
  return taint.propagate(
      /* callee */ nullptr,
      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
      call_position,
      options.maximum_source_sink_distance(),
      /* extra_features */ {},
      context,
      /* source_register_types */ {},
      /* source_constant_arguments */ {},
      /* class_interval_context */ CallClassIntervalContext(),
      /* caller_class_interval */ ClassIntervals::Interval::top());
}

} // namespace

Taint MethodContext::field_sources_at_callsite(
    const FieldTarget& field_target,
    const InstructionAliasResults& aliasing) const {
  auto declared_field_model = registry.get(field_target.field);
  if (declared_field_model.empty()) {
    return Taint::bottom();
  }

  auto* call_position = positions.get(method(), aliasing.position());
  return propagate_field_taint(
      declared_field_model.sources(), call_position, options, context_);
}

Taint MethodContext::field_sinks_at_callsite(
    const FieldTarget& field_target,
    const InstructionAliasResults& aliasing) const {
  auto declared_field_model = registry.get(field_target.field);
  if (declared_field_model.empty()) {
    return Taint::bottom();
  }

  auto* call_position = positions.get(method(), aliasing.position());
  return propagate_field_taint(
      declared_field_model.sinks(), call_position, options, context_);
}

} // namespace marianatrench
