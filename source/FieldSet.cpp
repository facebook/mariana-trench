/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/FieldSet.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

FieldSet::FieldSet(std::initializer_list<const Field*> fields) : set_(fields) {}

FieldSet::FieldSet(const Fields& fields) : set_(fields.begin(), fields.end()) {}

void FieldSet::add(const Field* field) {
  if (is_top_) {
    return;
  }
  set_.insert(field);
}

void FieldSet::remove(const Field* field) {
  if (is_top_) {
    return;
  }
  set_.remove(field);
}

bool FieldSet::leq(const FieldSet& other) const {
  if (is_top_) {
    return other.is_top_;
  }
  if (other.is_top_) {
    return true;
  }
  return set_.is_subset_of(other.set_);
}

bool FieldSet::equals(const FieldSet& other) const {
  return is_top_ == other.is_top_ && set_.equals(other.set_);
}

void FieldSet::join_with(const FieldSet& other) {
  if (is_top_) {
    return;
  }
  if (other.is_top_) {
    set_to_top();
    return;
  }
  set_.union_with(other.set_);
}

void FieldSet::widen_with(const FieldSet& other) {
  join_with(other);
}

void FieldSet::meet_with(const FieldSet& other) {
  if (is_top_) {
    *this = other;
    return;
  }
  if (other.is_top_) {
    return;
  }
  set_.intersection_with(other.set_);
}

void FieldSet::narrow_with(const FieldSet& other) {
  meet_with(other);
}

void FieldSet::difference_with(const FieldSet& other) {
  if (other.is_top_) {
    set_to_bottom();
    return;
  }
  if (is_top_) {
    return;
  }
  set_.difference_with(other.set_);
}

std::ostream& operator<<(std::ostream& out, const FieldSet& fields) {
  if (fields.is_top_) {
    return out << "T";
  }
  out << "{";
  for (auto iterator = fields.begin(), end = fields.end(); iterator != end;) {
    out << "`" << show(*iterator) << "`";
    ++iterator;
    if (iterator != end) {
      out << ", ";
    }
  }
  return out << "}";
}

} // namespace marianatrench
