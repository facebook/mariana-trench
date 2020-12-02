// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <Show.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/MethodSet.h>

namespace marianatrench {

MethodSet::MethodSet(std::initializer_list<const Method*> methods)
    : set_(methods) {}

void MethodSet::add(const Method* method) {
  set_.insert(method);
}

void MethodSet::remove(const Method* method) {
  set_.remove(method);
}

bool MethodSet::leq(const MethodSet& other) const {
  return set_.is_subset_of(other.set_);
}

bool MethodSet::equals(const MethodSet& other) const {
  return set_.equals(other.set_);
}

void MethodSet::join_with(const MethodSet& other) {
  set_.union_with(other.set_);
}

void MethodSet::widen_with(const MethodSet& other) {
  join_with(other);
}

void MethodSet::meet_with(const MethodSet& other) {
  set_.intersection_with(other.set_);
}

void MethodSet::narrow_with(const MethodSet& other) {
  meet_with(other);
}

MethodSet MethodSet::from_json(const Json::Value& value, Context& context) {
  MethodSet methods;
  for (const auto& method_value : JsonValidation::null_or_array(value)) {
    methods.add(Method::from_json(method_value, context));
  }
  return methods;
}

Json::Value MethodSet::to_json() const {
  auto methods = Json::Value(Json::arrayValue);
  for (const auto* method : set_) {
    methods.append(method->to_json());
  }
  return methods;
}

std::ostream& operator<<(std::ostream& out, const MethodSet& methods) {
  out << "{";
  for (auto iterator = methods.begin(), end = methods.end(); iterator != end;) {
    out << "`" << show(*iterator) << "`";
    ++iterator;
    if (iterator != end) {
      out << ", ";
    }
  }
  return out << "}";
}

} // namespace marianatrench
