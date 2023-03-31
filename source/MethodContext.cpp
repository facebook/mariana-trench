/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/MethodContext.h>

#include <Show.h>

#include <mariana-trench/Features.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Overrides.h>

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
      overrides(*context.overrides),
      call_graph(*context.call_graph),
      rules(*context.rules),
      dependencies(*context.dependencies),
      scheduler(*context.scheduler),
      kinds(*context.kinds),
      features(*context.features),
      registry(registry),
      transforms(*context.transforms),
      used_kinds(*context.used_kinds),
      memory_factory(previous_model.method()),
      previous_model(previous_model),
      new_model(new_model),
      context_(context) {
  std::string method_name = show(previous_model.method());
  const auto& log_methods = options.log_methods();
  dump_ = std::any_of(
      log_methods.begin(), log_methods.end(), [&](const auto& pattern) {
        return method_name.find(pattern) != std::string::npos;
      });
}

Model MethodContext::model_at_callsite(
    const CallTarget& call_target,
    const Position* position,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments)
    const {
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
                       source_constant_arguments);

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
        source_constant_arguments);
    LOG_OR_DUMP(
        this,
        5,
        "Joining with model for `{}`: {}",
        show(override),
        override_model);
    model.join_with(override_model);
  }

  model.approximate(
      FeatureMayAlwaysSet{context_.features->get_widen_broadening_feature()});

  callsite_model_cache_.emplace(CacheKey{call_target, position}, model);
  return model;
}

} // namespace marianatrench
