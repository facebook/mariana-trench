/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <SpartaWorkQueue.h>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/UsedKinds.h>

namespace marianatrench {

std::unordered_set<const Kind*> UsedKinds::remove_unused_kinds(
    const Rules& rules,
    const Kinds& kinds,
    const Methods& methods,
    ArtificialMethods& artificial_methods,
    Registry& registry) {
  auto unused_kinds = rules.collect_unused_kinds(kinds);
  auto queue = sparta::work_queue<const Method*>(
      [&](const Method* method) {
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
  return unused_kinds;
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

} // namespace

UsedKinds UsedKinds::from_rules(
    const Rules& rules,
    const Transforms& transforms) {
  UsedKinds used_kinds;

  std::unordered_map<const Kind*, std::unordered_set<const TransformKind*>>
      source_to_sink{};
  for (const auto& [source, sink_to_rules] : rules.source_to_sink_rules()) {
    for (const auto& [rule_sink, _] : sink_to_rules) {
      const auto* sink_transform_kind = rule_sink->as<TransformKind>();
      if (sink_transform_kind == nullptr) {
        continue;
      }

      const auto* sink = sink_transform_kind->base_kind();
      const auto* rule_transform = sink_transform_kind->local_transforms();
      const auto* rule_transform_reverse = transforms.reverse(rule_transform);
      mt_assert(rule_transform != nullptr && rule_transform_reverse != nullptr);

      add_source_sink_transforms(
          used_kinds.named_kind_to_transforms_, source, rule_transform_reverse);
      add_source_sink_transforms(
          used_kinds.named_kind_to_transforms_, sink, rule_transform);
      add_propagation_transforms(
          used_kinds.propagation_kind_to_transforms_, rule_transform);

      auto combinations = transforms.all_combinations(rule_transform);
      for (const auto& [left, right] : combinations.partitions) {
        const TransformList* left_reverse = transforms.reverse(left);

        // Add all valid sink transforms
        add_source_sink_transforms(
            used_kinds.named_kind_to_transforms_, sink, right);

        // Add all valid source transforms
        add_source_sink_transforms(
            used_kinds.named_kind_to_transforms_, source, left_reverse);

        // Add all valid propagation transforms
        add_propagation_transforms(
            used_kinds.propagation_kind_to_transforms_, left);
        add_propagation_transforms(
            used_kinds.propagation_kind_to_transforms_, right);
      }

      for (const auto* inner_transforms : combinations.subsequences) {
        add_propagation_transforms(
            used_kinds.propagation_kind_to_transforms_, inner_transforms);
      }
    }
  }

  return used_kinds;
}

} // namespace marianatrench
