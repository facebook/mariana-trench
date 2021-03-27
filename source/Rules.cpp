/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kinds.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/SourceSinkRule.h>

namespace marianatrench {

Rules::Rules() = default;

Rules::Rules(Context& context, std::vector<std::unique_ptr<Rule>> rules) {
  for (auto& rule : rules) {
    add(context, std::move(rule));
  }
}

Rules::Rules(Context& context, const Json::Value& rules_value) {
  for (const auto& rule_value : JsonValidation::null_or_array(rules_value)) {
    add(context, Rule::from_json(rule_value, context));
  }
}

Rules Rules::load(Context& context, const Options& options) {
  Rules rules;

  for (const auto& rules_path : options.rules_paths()) {
    auto rules_value = JsonValidation::parse_json_file(rules_path);
    for (const auto& rule_value : JsonValidation::null_or_array(rules_value)) {
      rules.add(context, Rule::from_json(rule_value, context));
    }
  }

  rules.warn_unused_kinds(*context.kinds);
  return rules;
}

void Rules::add(Context& context, std::unique_ptr<Rule> rule) {
  auto existing = rules_.find(rule->code());
  if (existing != rules_.end()) {
    ERROR(
        1,
        "A rule for code {} already exists! Duplicate rules are:\n{}\n{}",
        rule->code(),
        JsonValidation::to_styled_string(rule->to_json()),
        JsonValidation::to_styled_string(existing->second->to_json()));
    return;
  }

  auto code = rule->code();
  auto result = rules_.emplace(code, std::move(rule));
  const Rule* rule_pointer = result.first->second.get();

  if (auto* source_sink_rule = rule_pointer->as<SourceSinkRule>()) {
    for (const auto* source_kind : source_sink_rule->source_kinds()) {
      for (const auto* sink_kind : source_sink_rule->sink_kinds()) {
        source_to_sink_to_rules_[source_kind][sink_kind].push_back(
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
              context.kinds->get_triggered(sink_kind, multi_source_rule);
          source_to_partial_sink_to_rules_[source_kind][sink_kind].push_back(
              multi_source_rule);
          source_to_sink_to_rules_[source_kind][triggered].push_back(
              multi_source_rule);
        }
      }
    }
  } else {
    // Unreachable code. Did we add a new type of rule?
    mt_unreachable();
  }
}

const std::vector<const Rule*>& Rules::rules(
    const Kind* source_kind,
    const Kind* sink_kind) const {
  auto sink_to_rules = source_to_sink_to_rules_.find(source_kind);
  if (sink_to_rules == source_to_sink_to_rules_.end()) {
    return empty_rule_set_;
  }

  auto rules = sink_to_rules->second.find(sink_kind);
  if (rules == sink_to_rules->second.end()) {
    return empty_rule_set_;
  }

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

void Rules::warn_unused_kinds(const Kinds& kinds) const {
  for (const auto* kind : kinds.kinds()) {
    if (kind->as<TriggeredPartialKind>() != nullptr) {
      // Triggered kinds are never used in rules. No need to warn.
      continue;
    }
    if (std::all_of(begin(), end(), [kind](const Rule* rule) {
          return !rule->uses(kind);
        })) {
      WARNING(
          1,
          "Kind `{}` is not used in any rule! You may want to add one for it.",
          show(kind));
    }
  }
}

} // namespace marianatrench
