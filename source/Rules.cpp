/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/SourceSinkRule.h>
#include <mariana-trench/SourceSinkWithExploitabilityRule.h>

namespace marianatrench {

Rules::Rules(Context& context, std::vector<std::unique_ptr<Rule>> rules)
    : transforms_factory(*context.transforms_factory),
      kind_factory(*context.kind_factory) {
  for (auto& rule : rules) {
    add(context, std::move(rule));
  }
}

Rules::Rules(Context& context, const Json::Value& rules_value)
    : transforms_factory(*context.transforms_factory),
      kind_factory(*context.kind_factory) {
  for (const auto& rule_value : JsonValidation::null_or_array(rules_value)) {
    add(context, Rule::from_json(rule_value, context));
  }
}

Rules Rules::load(Context& context, const Options& options) {
  Rules rules(context);

  for (const auto& rules_path : options.rules_paths()) {
    auto rules_value = JsonReader::parse_json_file(rules_path);
    for (const auto& rule_value : JsonValidation::null_or_array(rules_value)) {
      rules.add(context, Rule::from_json(rule_value, context));
    }
  }

  return rules;
}

void Rules::add(Context& context, std::unique_ptr<Rule> rule) {
  auto existing = rules_.find(rule->code());
  if (existing != rules_.end()) {
    ERROR(
        1,
        "A rule for code {} already exists! Duplicate rules are:\n{}\n{}",
        rule->code(),
        JsonWriter::to_styled_string(rule->to_json()),
        JsonWriter::to_styled_string(existing->second->to_json()));
    return;
  }

  auto code = rule->code();
  auto result = rules_.emplace(code, std::move(rule));
  const Rule* rule_pointer = result.first->second.get();

  if (auto* source_sink_rule = rule_pointer->as<SourceSinkRule>()) {
    for (const auto* source_kind : source_sink_rule->source_kinds()) {
      for (const auto* sink_kind : source_sink_rule->sink_kinds()) {
        const auto* transforms = source_sink_rule->transform_kinds();
        if (transforms == nullptr) {
          source_to_sink_to_rules_[source_kind][sink_kind].push_back(
              rule_pointer);
          continue;
        }

        const auto* sink_transform_kind = context.kind_factory->transform_kind(
            sink_kind,
            /* local transforms */ transforms,
            /* global transforms */ nullptr);

        source_to_sink_to_rules_[source_kind][sink_transform_kind].push_back(
            rule_pointer);
      }
    }
  } else if (
      const auto* multi_source_rule =
          rule_pointer->as<MultiSourceMultiSinkRule>()) {
    // Consider the rule:
    //   Code: 1000
    //   Sources: { lblA: [SourceA], lblB: [SourceB] }
    //   Sinks: [ Partial(SinkX, lblA), Partial(SinkX, lblB) ]
    //
    // A flow like SourceA -> Partial(SinkX, lblA) fulfills half the rule
    // (tracked in `source_to_partial_sink_to_rules_`).
    //
    // When a half-fulfilled rule is seen, the analysis creates a triggered
    // partial sink:
    //   B = Triggered(SinkX, lblB, rule: 1000)
    //
    // The rule is completely fulfilled when "SourceB -> B" is detected,
    // which is what `source_to_sink_to_rules_` tracks.
    //
    // Tracking the rule in the triggered sink is necessary because there can
    // be another rule with the same sinks but different sources:
    //   Code: 2000
    //   Sources: { lblA: [SourceC], lblB: [SourceB] }
    //   Sinks: [ Partial(SinkX, lblA), Partial(SinkX, lblB) ]
    //
    // Without the rule, we cannot tell which rule is satisfied:
    //   SourceB -> Triggered(SinkX, lblB)  can match either rule.
    // With the rule, it is clear that only the first rule applies:
    //   SourceB -> Triggered(SinkX, lblB, rule: 1000)

    for (const auto& [source_label, source_kinds] :
         multi_source_rule->multi_source_kinds()) {
      for (const auto* source_kind : source_kinds) {
        for (const auto* sink_kind :
             multi_source_rule->partial_sink_kinds(source_label)) {
          const auto* triggered =
              context.kind_factory->get_triggered(sink_kind, multi_source_rule);
          source_to_partial_sink_to_rules_[source_kind][sink_kind].push_back(
              multi_source_rule);
          source_to_sink_to_rules_[source_kind][triggered].push_back(
              multi_source_rule);
        }
      }
    }
  } else if (
      const auto* exploitability_rule =
          rule_pointer->as<SourceSinkWithExploitabilityRule>()) {
    for (const auto* source_kind : exploitability_rule->source_kinds()) {
      for (const auto* sink_kind : exploitability_rule->sink_kinds()) {
        // Create normal source to sink rule for initial matching.
        source_to_sink_to_exploitability_rules_[source_kind][sink_kind]
            .push_back(exploitability_rule);

        // Create rule for effect-source to sink as a transform kind with
        // source-as-transform local transforms for final matching.
        const auto* source_as_transform_sink_kind =
            context.kind_factory->transform_kind(
                sink_kind,
                /* local transforms */
                context.transforms_factory->create(
                    TransformList::from_kind(source_kind, context)),
                /* global transforms */ nullptr);
        for (const auto* effect_source_kind :
             exploitability_rule->effect_source_kinds()) {
          effect_source_to_sink_to_exploitability_rules_
              [effect_source_kind][source_as_transform_sink_kind]
                  .push_back(exploitability_rule);
        }
      }
    }

  } else {
    // Unreachable code. Did we add a new type of rule?
    mt_unreachable();
  }
}

namespace {

const Kind* MT_NULLABLE canonicalize_sink_kind(
    const KindFactory& kind_factory,
    const TransformsFactory& transforms_factory,
    const Kind* source_kind,
    const Kind* sink_kind) {
  // Get a list of all transforms (if any)
  const TransformList* all_transforms = nullptr;
  if (const auto* source_transform_kind = source_kind->as<TransformKind>()) {
    all_transforms = transforms_factory.concat(
        source_transform_kind->local_transforms(),
        source_transform_kind->global_transforms());
    all_transforms = transforms_factory.reverse(all_transforms);
  }

  if (const auto* sink_transform_kind = sink_kind->as<TransformKind>()) {
    const auto* all_sink_transforms = transforms_factory.concat(
        sink_transform_kind->local_transforms(),
        sink_transform_kind->global_transforms());
    all_transforms =
        transforms_factory.concat(all_transforms, all_sink_transforms);
  }

  if (all_transforms != nullptr) {
    const auto* source_base_kind = source_kind->discard_transforms();
    const auto* sink_base_kind = sink_kind->discard_transforms();

    // Check if we can sanitize the source
    if (all_transforms->sanitizes(
            source_base_kind, TransformList::ApplicationDirection::Forward)) {
      return nullptr;
    }

    // Check if we can sanitize the sink
    if (all_transforms->sanitizes(
            sink_base_kind, TransformList::ApplicationDirection::Backward)) {
      return nullptr;
    }

    // Discard sanitizing transforms for rules matching
    all_transforms = transforms_factory.discard_sanitizers(all_transforms);

    // Every transform is a sanitizer
    if (all_transforms == nullptr) {
      sink_kind = sink_base_kind;
    } else {
      sink_kind = kind_factory.transform_kind(
          sink_base_kind,
          /* local transforms */ all_transforms,
          /* global transforms */ nullptr);
    }
  }

  return sink_kind;
}

} // namespace

const std::vector<const Rule*>& Rules::rules(
    const Kind* source_kind,
    const Kind* sink_kind) const {
  LOG(4,
      "Searching for source-sink rules matching source: {} -> sink: {}",
      source_kind->to_trace_string(),
      sink_kind->to_trace_string());

  auto sink_to_rules =
      source_to_sink_to_rules_.find(source_kind->discard_transforms());
  if (sink_to_rules == source_to_sink_to_rules_.end()) {
    return empty_rule_set_;
  }

  const auto* canonicalized_sink_kind = canonicalize_sink_kind(
      kind_factory, transforms_factory, source_kind, sink_kind);

  if (canonicalized_sink_kind == nullptr) {
    return empty_rule_set_;
  }

  auto rules = sink_to_rules->second.find(canonicalized_sink_kind);
  if (rules == sink_to_rules->second.end()) {
    return empty_rule_set_;
  }

  LOG(4,
      "Found rule match for: {}->{} ",
      source_kind->discard_transforms()->to_trace_string(),
      canonicalized_sink_kind->to_trace_string());

  return rules->second;
}

const std::vector<const SourceSinkWithExploitabilityRule*>&
Rules::partially_fulfilled_exploitability_rules(
    const Kind* source_kind,
    const Kind* sink_kind) const {
  LOG(4,
      "Searching for partially fulfilled exploitability rules matching source: {} -> sink: {}",
      source_kind->to_trace_string(),
      sink_kind->to_trace_string());

  auto sink_to_rules = source_to_sink_to_exploitability_rules_.find(
      source_kind->discard_transforms());
  if (sink_to_rules == source_to_sink_to_exploitability_rules_.end()) {
    return empty_exploitability_rule_set_;
  }

  const auto* canonicalized_sink_kind = canonicalize_sink_kind(
      kind_factory, transforms_factory, source_kind, sink_kind);

  if (canonicalized_sink_kind == nullptr) {
    return empty_exploitability_rule_set_;
  }

  auto rules = sink_to_rules->second.find(canonicalized_sink_kind);
  if (rules == sink_to_rules->second.end()) {
    return empty_exploitability_rule_set_;
  }

  LOG(4,
      "Found partially fulfilled exploitability rule match for: {}->{} ",
      source_kind->discard_transforms()->to_trace_string(),
      canonicalized_sink_kind->to_trace_string());

  return rules->second;
}

const std::vector<const SourceSinkWithExploitabilityRule*>&
Rules::fulfilled_exploitability_rules(
    const Kind* effect_source_kind,
    const TransformKind* source_as_transform_sink_kind) const {
  LOG(4,
      "Searching for fulfilled exploitability rules matching effect source: {} -> sink: {}",
      effect_source_kind->to_trace_string(),
      source_as_transform_sink_kind->to_trace_string());

  mt_assert(effect_source_kind->is<NamedKind>());
  auto sink_to_rules =
      effect_source_to_sink_to_exploitability_rules_.find(effect_source_kind);
  if (sink_to_rules == effect_source_to_sink_to_exploitability_rules_.end()) {
    return empty_exploitability_rule_set_;
  }

  const auto* canonicalized_sink_kind = canonicalize_sink_kind(
      kind_factory,
      transforms_factory,
      effect_source_kind,
      source_as_transform_sink_kind);
  mt_assert(canonicalized_sink_kind != nullptr);
  const auto* source_as_transform_sink =
      canonicalized_sink_kind->as<TransformKind>();
  mt_assert(
      source_as_transform_sink != nullptr &&
      source_as_transform_sink->has_source_as_transform());
  auto rules = sink_to_rules->second.find(source_as_transform_sink);
  if (rules == sink_to_rules->second.end()) {
    return empty_exploitability_rule_set_;
  }

  LOG(4,
      "Found fulfilled exploitability rule match for: {}->{} ",
      effect_source_kind->to_trace_string(),
      source_as_transform_sink_kind->to_trace_string());

  return rules->second;
}

const std::vector<const MultiSourceMultiSinkRule*>& Rules::partial_rules(
    const Kind* source_kind,
    const PartialKind* sink_kind) const {
  auto sink_to_rules = source_to_partial_sink_to_rules_.find(source_kind);
  if (sink_to_rules == source_to_partial_sink_to_rules_.end()) {
    return empty_multi_source_rule_set_;
  }

  auto rules = sink_to_rules->second.find(sink_kind);
  if (rules == sink_to_rules->second.end()) {
    return empty_multi_source_rule_set_;
  }

  return rules->second;
}

std::unordered_set<const Kind*> Rules::collect_unused_kinds(
    const KindFactory& kind_factory) const {
  std::unordered_set<const Kind*> unused_kinds;
  for (const auto* kind : kind_factory.kinds()) {
    if (kind->is<TriggeredPartialKind>() || kind->is<PropagationKind>()) {
      // These kinds are never used in rules.
      continue;
    }
    if (std::all_of(begin(), end(), [kind](const Rule* rule) {
          return !rule->uses(kind);
        })) {
      unused_kinds.insert(kind);
      WARNING(
          1,
          "Kind `{}` is not used in any rule! You may want to add one for it.",
          show(kind));
    }
  }
  return unused_kinds;
}

} // namespace marianatrench
