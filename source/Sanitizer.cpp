/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/Sanitizer.h>

namespace marianatrench {

bool Sanitizer::leq(const Sanitizer& other) const {
  if (is_bottom()) {
    return true;
  } else if (other.is_bottom()) {
    return false;
  }
  return sanitizer_kind_ == other.sanitizer_kind_ && kinds_.leq(other.kinds_);
}

bool Sanitizer::equals(const Sanitizer& other) const {
  if (is_bottom()) {
    return other.is_bottom();
  } else if (other.is_bottom()) {
    return false;
  } else {
    return sanitizer_kind_ == other.sanitizer_kind_ && kinds_ == other.kinds_;
  }
}

void Sanitizer::join_with(const Sanitizer& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    *this = other;
  } else if (other.is_bottom()) {
    return;
  } else {
    mt_assert(sanitizer_kind_ == other.sanitizer_kind_);
    kinds_.join_with(other.kinds_);
  }

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void Sanitizer::widen_with(const Sanitizer& other) {
  join_with(other);
}

void Sanitizer::meet_with(const Sanitizer& /* other */) {
  mt_unreachable(); // Not implemented
}

void Sanitizer::narrow_with(const Sanitizer& other) {
  meet_with(other);
}

std::ostream& operator<<(std::ostream& out, const Sanitizer& sanitizer) {
  out << "Sanitizer(";
  switch (sanitizer.sanitizer_kind_) {
    case SanitizerKind::Sources:
      out << "Sources";
      break;
    case SanitizerKind::Sinks:
      out << "Sinks";
      break;
    case SanitizerKind::Propagations:
      out << "Propagations";
      break;
  }
  if (sanitizer.sanitizer_kind_ != SanitizerKind::Sinks) {
    out << ", kinds = " << sanitizer.kinds_;
  }
  return out << ")";
}

} // namespace marianatrench
