/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/TriggeredPartialKind.h>

namespace marianatrench {

void TriggeredPartialKind::show(std::ostream& out) const {
  out << "TriggeredPartial:" << partial_kind_->name() << ":"
      << partial_kind_->label() << ":" << rule_code_;
}

Json::Value TriggeredPartialKind::to_json() const {
  // JSON Format for TriggeredPartialKind is similar to the underlying
  // PartialKind's with the addition of the triggered rule. The triggered rule
  // is used for debugging, and also for differentiating between triggered and
  // non-triggered partial kinds in JSON parsing.
  auto nestedValue = partial_kind_->to_json()["kind"];
  nestedValue["triggered_rule"] = rule_code_;

  auto value = Json::Value(Json::objectValue);
  value["kind"] = nestedValue;

  return value;
}

const TriggeredPartialKind* TriggeredPartialKind::from_inner_json(
    const Json::Value& value,
    Context& context) {
  auto name = JsonValidation::string(value, /* field */ "name");
  auto label = JsonValidation::string(value, /* field */ "partial_label");
  auto rule_code = JsonValidation::integer(value, /* field */ "triggered_rule");

  // This assumes that the rule code from the input JSON is based on the same
  // set of rules as the current run. This is the case when using a global
  // rules.json configuration across runs but is error prone otherwise.
  return context.kind_factory->get_triggered(
      context.kind_factory->get_partial(name, "label"), rule_code);
}

std::string TriggeredPartialKind::to_trace_string() const {
  // String representation of TriggeredPartialKind is unused outside of
  // debugging. See mariana_trench_parser_objects.py where the JSON
  // representation is converted to a string.
  return "TriggeredPartial:" + partial_kind_->name() + ":" +
      partial_kind_->label() + ":" + rule_code_;
}

} // namespace marianatrench
