/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <sparta/WorkQueue.h>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/UsedKinds.h>

namespace marianatrench {

std::size_t UsedKinds::remove_unused_kinds(
    const Rules& rules,
    const KindFactory& kind_factory,
    const Methods& methods,
    ArtificialMethods& artificial_methods,
    Registry& registry) {
  auto unused_kinds = rules.collect_unused_kinds(kind_factory);
  auto queue = sparta::work_queue<const Method*>(
      [&registry, &unused_kinds](const Method* method) {
        auto model = registry.get(method);
        model.remove_kinds(unused_kinds);
        registry.set(model);
      },
      sparta::parallel::default_num_threads());
  for (const auto* method : methods) {
    queue.add_item(method);
  }
  queue.run_all();
  artificial_methods.set_unused_kinds(unused_kinds);
  return unused_kinds.size();
}

namespace {

void add_source_sink_transforms(
    UsedKinds::NamedKindToTransformsMap& named_kind_to_transforms,
    const Kind* kind,
    const TransformList* MT_NULLABLE transforms) {
  if (transforms == nullptr) {
    return;
  }
  named_kind_to_transforms[kind].insert(transforms);
}

void add_propagation_transforms(
    UsedKinds::PropagationKindTransformsSet& propagation_kind_to_transforms,
    const TransformList* MT_NULLABLE transforms) {
  if (transforms == nullptr) {
    return;
  }
  propagation_kind_to_transforms.insert(transforms);
}

void add_used_kinds(
    const TransformsFactory& transforms_factory,
    UsedKinds::NamedKindToTransformsMap& named_kind_to_transforms,
    UsedKinds::PropagationKindTransformsSet& propagation_kind_to_transforms,
    const Kind* source,
    const Kind* rule_sink) {
  const auto* sink_transform_kind = rule_sink->as<TransformKind>();
  if (sink_transform_kind == nullptr) {
    return;
  }

  const auto* sink = sink_transform_kind->base_kind();
  const auto* rule_transform = sink_transform_kind->local_transforms();
  const auto* rule_transform_reverse =
      transforms_factory.reverse(rule_transform);
  mt_assert(rule_transform != nullptr && rule_transform_reverse != nullptr);

  add_source_sink_transforms(
      named_kind_to_transforms, source, rule_transform_reverse);
  add_source_sink_transforms(named_kind_to_transforms, sink, rule_transform);
  add_propagation_transforms(propagation_kind_to_transforms, rule_transform);

  auto combinations = transforms_factory.all_combinations(rule_transform);
  for (const auto& [left, right] : combinations.partitions) {
    const TransformList* left_reverse = transforms_factory.reverse(left);

    // Add all valid sink transforms
    add_source_sink_transforms(named_kind_to_transforms, sink, right);

    // Add all valid source transforms
    add_source_sink_transforms(named_kind_to_transforms, source, left_reverse);

    // Add all valid propagation transforms
    add_propagation_transforms(propagation_kind_to_transforms, left);
    add_propagation_transforms(propagation_kind_to_transforms, right);
  }

  for (const auto* inner_transforms : combinations.subsequences) {
    add_propagation_transforms(
        propagation_kind_to_transforms, inner_transforms);
  }
}

} // namespace

UsedKinds UsedKinds::from_rules(
    const Rules& rules,
    const TransformsFactory& transforms_factory) {
  UsedKinds used_kinds(transforms_factory);

  // Add used kinds in source to sink rules
  for (const auto& [source, sink_to_rules] : rules.source_to_sink_rules()) {
    for (const auto& [rule_sink, _] : sink_to_rules) {
      add_used_kinds(
          transforms_factory,
          used_kinds.named_kind_to_transforms_,
          used_kinds.propagation_kind_to_transforms_,
          source,
          rule_sink);
    }
  }

  // Add used kinds from source to sink exploitability rules
  for (const auto& [source, sink_to_rules] :
       rules.source_to_sink_exploitability_rules()) {
    for (const auto& [rule_sink, _] : sink_to_rules) {
      add_used_kinds(
          transforms_factory,
          used_kinds.named_kind_to_transforms_,
          used_kinds.propagation_kind_to_transforms_,
          source,
          rule_sink);
    }
  }

  // Add used kinds from effect source to sink exploitability rules
  for (const auto& [source, sink_to_rules] :
       rules.effect_source_to_sink_exploitability_rules()) {
    for (const auto& [rule_sink, _] : sink_to_rules) {
      add_used_kinds(
          transforms_factory,
          used_kinds.named_kind_to_transforms_,
          used_kinds.propagation_kind_to_transforms_,
          source,
          rule_sink);
    }
  }

  return used_kinds;
}

bool UsedKinds::should_keep(const TransformKind* transform_kind) const {
  const auto* base_kind = transform_kind->base_kind();

  const auto* transforms_to_check =
      transforms_factory_.discard_sanitizers(transforms_factory_.concat(
          transform_kind->local_transforms(),
          transform_kind->global_transforms()));

  // Since TransformFactory::concat() cannot return nullptr, the only case where
  // TransformFactory::discard_sanitizers() can return nullptr is when the input
  // transforms only contains sanitizers, and under this case we do not want to
  // drop the them.
  if (transforms_to_check == nullptr) {
    return true;
  }

  if (base_kind->is<PropagationKind>()) {
    return propagation_kind_to_transforms_.find(transforms_to_check) !=
        propagation_kind_to_transforms_.end();
  }

  const auto& valid_transforms = named_kind_to_transforms_.find(base_kind);
  if (valid_transforms == named_kind_to_transforms_.end()) {
    return false;
  }

  return valid_transforms->second.find(transforms_to_check) !=
      valid_transforms->second.end();
}

} // namespace marianatrench
