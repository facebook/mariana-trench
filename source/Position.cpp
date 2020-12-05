/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Position.h>
#include <mariana-trench/Positions.h>

namespace marianatrench {

namespace {

constexpr int k_unknown_line = -1;

} // namespace

Position::Position(const std::string* path, int line)
    : path_(path), line_(line) {}

bool Position::operator==(const Position& other) const {
  return path_ == other.path_ && line_ == other.line_;
}

bool Position::operator!=(const Position& other) const {
  return !this->operator==(other);
}

std::ostream& operator<<(std::ostream& out, const Position& position) {
  out << "Position(";
  if (position.path_ != nullptr) {
    out << "path=`" << show(position.path_) << "`";
    if (position.line_ != k_unknown_line) {
      out << ", ";
    }
  }
  if (position.line_ != k_unknown_line) {
    out << "line=" << position.line_;
  }
  return out << ")";
}

const Position* Position::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  int line = k_unknown_line;
  if (value.isMember("line")) {
    line = JsonValidation::integer(value, /* field */ "line");
  }

  std::optional<std::string> path;
  if (value.isMember("path")) {
    path = JsonValidation::string(value, /* field */ "path");
  }

  return context.positions->get(path, line);
}

Json::Value Position::to_json(bool with_path) const {
  auto value = Json::Value(Json::objectValue);
  if (line_ != k_unknown_line) {
    value["line"] = Json::Value(line_);
  }
  if (with_path && path_) {
    value["path"] = Json::Value(*path_);
  }
  return value;
}

} // namespace marianatrench
