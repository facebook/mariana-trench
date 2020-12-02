// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <mariana-trench/Rule.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

Rule Rule::from_json(const Json::Value& value, Context& context) {
  JsonValidation::validate_object(value);

  auto name = JsonValidation::string(value, /* field */ "name");
  int code = JsonValidation::integer(value, /* field */ "code");
  auto description = JsonValidation::string(value, /* field */ "description");

  std::unordered_set<const Kind*> source_kinds;
  for (const auto& source_kind :
       JsonValidation::nonempty_array(value, /* field */ "sources")) {
    source_kinds.insert(Kind::from_json(source_kind, context));
  }

  std::unordered_set<const Kind*> sink_kinds;
  for (const auto& sink_kind :
       JsonValidation::nonempty_array(value, /* field */ "sinks")) {
    sink_kinds.insert(Kind::from_json(sink_kind, context));
  }

  return Rule(name, code, description, source_kinds, sink_kinds);
}

Json::Value Rule::to_json() const {
  auto source_kinds_value = Json::Value(Json::arrayValue);
  for (const auto source_kind : source_kinds_) {
    source_kinds_value.append(source_kind->to_json());
  }

  auto sink_kinds_value = Json::Value(Json::arrayValue);
  for (const auto sink_kind : sink_kinds_) {
    sink_kinds_value.append(sink_kind->to_json());
  }

  auto value = Json::Value(Json::objectValue);
  value["name"] = name_;
  value["code"] = Json::Value(code_);
  value["description"] = description_;
  value["sources"] = source_kinds_value;
  value["sinks"] = sink_kinds_value;
  return value;
}

} // namespace marianatrench
