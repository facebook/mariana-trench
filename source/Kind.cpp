// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <mariana-trench/Kind.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kinds.h>

namespace marianatrench {

const Kind* Kind::from_json(const Json::Value& value, Context& context) {
  return context.kinds->get(JsonValidation::string(value));
}

Json::Value Kind::to_json() const {
  return Json::Value(name_);
}

std::ostream& operator<<(std::ostream& out, const Kind& kind) {
  return out << kind.name_;
}

} // namespace marianatrench
