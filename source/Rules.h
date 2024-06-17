/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include <boost/iterator/transform_iterator.hpp>
#include <json/json.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/PartialKind.h>
#include <mariana-trench/Rule.h>
#include <mariana-trench/SourceSinkWithExploitabilityRule.h>
#include <mariana-trench/TransformKind.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

class Rules final {
 public:
  using SourceSinkRulesMap = std::unordered_map<
      const Kind*,
      std::unordered_map<const Kind*, std::vector<const Rule*>>>;
  using SourcePartialSinkRulesMap = std::unordered_map<
      const Kind*,
      std::unordered_map<
          const PartialKind*,
          std::vector<const MultiSourceMultiSinkRule*>>>;
  using SourceSinkExploitabilityRulesMap = std::unordered_map<
      const Kind*,
      std::unordered_map<
          const Kind*,
          std::vector<const SourceSinkWithExploitabilityRule*>>>;
  using EffectSourceSinkExploitabilityRulesMap = std::unordered_map<
      const Kind*,
      std::unordered_map<
          const TransformKind*,
          std::vector<const SourceSinkWithExploitabilityRule*>>>;

 private:
  struct ExposeRulePointer {
    const Rule* operator()(
        const std::pair<const int, std::unique_ptr<Rule>>& pair) const {
      return pair.second.get();
    }
  };

 public:
  // C++ container concept member types
  using iterator = boost::transform_iterator<
      ExposeRulePointer,
      std::unordered_map<int, std::unique_ptr<Rule>>::const_iterator>;
  using const_iterator = iterator;
  using value_type = const Rule*;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const Rule*;
  using const_pointer = typename iterator::pointer;

 private:
  // Safety checks of `boost::transform_iterator`.
  static_assert(std::is_same_v<typename iterator::value_type, value_type>);
  static_assert(
      std::is_same_v<typename iterator::difference_type, difference_type>);
  static_assert(std::is_same_v<typename iterator::reference, const_reference>);

 public:
  explicit Rules(Context& context)
      : transforms_factory(*context.transforms_factory),
        kind_factory(*context.kind_factory) {}

  explicit Rules(Context& context, std::vector<std::unique_ptr<Rule>> rules);

  explicit Rules(Context& context, const Json::Value& rules_value);

  /* Load the rules from the json file specified in the given options. */
  static Rules load(Context& context, const Options& options);

  MOVE_CONSTRUCTOR_ONLY(Rules);

  /* This is NOT thread-safe. */
  void add(Context& context, std::unique_ptr<Rule> rule);

  /**
   * Return the set of rules matching the given source kind and sink kind.
   * Satisfying these rules should result in the creation of an issue (this
   * is the responsibility of the caller).
   */
  const std::vector<const Rule*>& rules(
      const Kind* source_kind,
      const Kind* sink_kind) const;

  /**
   * Return the set of rules matching the given source kind and sink kind.
   * Only for multi-source/sink rules. The rule is partially satisfied. The
   * other sink in the rule still needs to be met with its corresponding source
   * for an issue to be created (also the responsibility of the caller).
   */
  const std::vector<const MultiSourceMultiSinkRule*>& partial_rules(
      const Kind* source_kind,
      const PartialKind* sink_kind) const;

  /**
   * Return the set of partially fulfilled exploitability rules matching the
   * given source kind and sink kind. Exploitability rule is said to be
   * partially fulfilled when a kind specified as "sources" is found to flow
   * into a kind specified as "sinks" in the rule definition. It is the
   * responsibility of the caller to create the corresponding
   * source-as-transform sinks.
   */
  const std::vector<const SourceSinkWithExploitabilityRule*>&
  partially_fulfilled_exploitability_rules(
      const Kind* source_kind,
      const Kind* sink_kind) const;

  /**
   * Return the set of fulfilled exploitability rules matching the
   * given exploitability_source and source_as_transform_sink kind.
   * Exploitability rule is said to be fulfilled when the kind listed
   * "effect_sources" is found to flow to the special
   * "source-as-transform-sinks" materialized when the rule is
   * partially fulfulled. It is the responsibility of the caller to create an
   * issue.
   */
  const std::vector<const SourceSinkWithExploitabilityRule*>&
  fulfilled_exploitability_rules(
      const Kind* source_kind,
      const TransformKind* source_as_transform_sinks) const;

  std::unordered_set<const Kind*> collect_unused_kinds(
      const KindFactory& kinds) const;

  const SourceSinkRulesMap& source_to_sink_rules() const {
    return source_to_sink_to_rules_;
  }

  const SourceSinkExploitabilityRulesMap& source_to_sink_exploitability_rules()
      const {
    return source_to_sink_to_exploitability_rules_;
  }

  const EffectSourceSinkExploitabilityRulesMap&
  effect_source_to_sink_exploitability_rules() const {
    return effect_source_to_sink_to_exploitability_rules_;
  }

  std::size_t size() const {
    return rules_.size();
  }

  iterator begin() const {
    return boost::make_transform_iterator(rules_.cbegin(), ExposeRulePointer());
  }

  iterator end() const {
    return boost::make_transform_iterator(rules_.cend(), ExposeRulePointer());
  }

 private:
  const TransformsFactory& transforms_factory;
  const KindFactory& kind_factory;
  std::unordered_map<int, std::unique_ptr<Rule>> rules_;
  // For Rules with "transforms":
  //   Outer source kind = Kind without any Transforms
  //   Inner sink kind = Kind with all Transforms
  SourceSinkRulesMap source_to_sink_to_rules_;
  SourceSinkExploitabilityRulesMap source_to_sink_to_exploitability_rules_;
  EffectSourceSinkExploitabilityRulesMap
      effect_source_to_sink_to_exploitability_rules_;
  SourcePartialSinkRulesMap source_to_partial_sink_to_rules_;
  std::vector<const Rule*> empty_rule_set_;
  std::vector<const MultiSourceMultiSinkRule*> empty_multi_source_rule_set_;
  std::vector<const SourceSinkWithExploitabilityRule*>
      empty_exploitability_rule_set_;
};

} // namespace marianatrench
