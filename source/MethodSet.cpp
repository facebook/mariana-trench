/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/MethodSet.h>
#include <mariana-trench/Methods.h>

namespace marianatrench {

MethodSet::MethodSet(std::initializer_list<const Method*> methods)
    : set_(methods) {}

MethodSet::MethodSet(const Methods& methods)
    : set_(methods.begin(), methods.end()) {}

void MethodSet::add(const Method* method) {
  if (is_top_) {
    return;
  }
  set_.insert(method);
}

void MethodSet::remove(const Method* method) {
  if (is_top_) {
    return;
  }
  set_.remove(method);
}

bool MethodSet::leq(const MethodSet& other) const {
  if (is_top_) {
    return other.is_top_;
  }
  if (other.is_top_) {
    return true;
  }
  return set_.is_subset_of(other.set_);
}

bool MethodSet::equals(const MethodSet& other) const {
  return is_top_ == other.is_top_ && set_.equals(other.set_);
}

void MethodSet::join_with(const MethodSet& other) {
  if (is_top_) {
    return;
  }
  if (other.is_top_) {
    set_to_top();
    return;
  }
  set_.union_with(other.set_);
}

void MethodSet::widen_with(const MethodSet& other) {
  join_with(other);
}

void MethodSet::meet_with(const MethodSet& other) {
  if (is_top_) {
    *this = other;
    return;
  }
  if (other.is_top_) {
    return;
  }
  set_.intersection_with(other.set_);
}

void MethodSet::narrow_with(const MethodSet& other) {
  meet_with(other);
}

void MethodSet::difference_with(const MethodSet& other) {
  if (other.is_top_) {
    set_to_bottom();
    return;
  }
  if (is_top_) {
    return;
  }
  set_.difference_with(other.set_);
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
  if (methods.is_top_) {
    return out << "T";
  }
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
